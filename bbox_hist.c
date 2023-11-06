/* SPDX-License-Identifier: GPL-3.0-or-later */
#define _GNU_SOURCE
#include <stdio.h>
#include <err.h>
#include <sysexits.h>
#include <limits.h>
#include <getopt.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

/* increase for super-HD video */ 
#define MAX_WIDTH  1920
#define MAX_HEIGHT 1080

static char const *options = "-l:i:w:h:v:scd";
static struct option const long_options[] = {
	{ "max-luminance" , required_argument, NULL, 'l' },
	{ "min-incidence" , required_argument, NULL, 'i' },
	{ "width-factor"  , required_argument, NULL, 'w' },
	{ "height-factor" , required_argument, NULL, 'h' },
	{ "video"         , required_argument, NULL, 'v' },
	{ "save-histogram", no_argument      , NULL, 's' },
	{ "crop"          , no_argument      , NULL, 'c' },
	{ "drawbox"       , no_argument      , NULL, 'd' },
	{}
};

static unsigned    max_luminance = 32;
static double      min_incidence = 0.15; // of max incidence amplitude
static char const *video;
static char       *video_shell_quoted;
static unsigned    save_histogram, crop, drawbox;
static unsigned    width_factor, height_factor;
extern char       *optarg;

#define assign_allocptr( var, val )		\
	if( var ) free( var );				\
	var = val;

#define append_mem( dst, src, len )		\
	memcpy( dst, src, len );			\
	dst += (len);

char* alloc_printf( char const *templ, ... ) __attribute__(( format( printf, 1, 2 )));
char* alloc_printf( char const *templ, ... )
{
	char *buf;
	va_list va;
	va_start( va, templ );
	if( vasprintf( &buf, templ, va ) == -1 )
		err( EX_OSERR, "asprintf" );
	va_end( va );
	return buf;
}

typedef int (*parse_line_fn)( FILE *input, void *state );

void process_cmd( char *cmd_txt, parse_line_fn parse_line, void *state )
{
	FILE *cmd = popen( cmd_txt, "re" );
	if( !cmd ) err( EX_OSERR, "%s", cmd_txt );

	int c;
	for(;;) {
		c = getc_unlocked( cmd );
		if( c == EOF ) break;
		if( c != '\n' ) continue;
		c = parse_line( cmd, state );
		if( c ) break;
	}

	while( c != EOF ) c = getc_unlocked( cmd );
	if( ferror( cmd )) err( EX_IOERR, "%s", cmd_txt );
	
	free( cmd_txt );
	pclose( cmd );
}

int parse_duration( FILE *input, void *state )
{
	unsigned *p_duration = state;
	return fscanf( input, "duration=%u", p_duration );
}

unsigned video_duration()
{
	static char const cmd_tpl[] = "ffprobe -v error -hide_banner -show_entries format=duration %s";
	unsigned duration = 0;
	process_cmd( alloc_printf( cmd_tpl, video_shell_quoted ), &parse_duration, &duration );
	if( duration == 0 ) errx( EX_SOFTWARE, "cannot get duration" );
	return duration;
}

struct bbox_hist { unsigned x[ MAX_WIDTH+1 ], y[ MAX_HEIGHT+1 ]; }; // max stored @ last index

int parse_bbox( FILE *input, void *state )
{
	struct bbox_hist *p_hist = state;
	unsigned x1, x2, y1, y2;
	int c =	fscanf( input, "[Parsed_bbox_%*u @ %*i] n:%*u pts:%*u pts_time:%*u.%*u x1:%u x2:%u y1:%u y2:%u", &x1, &x2, &y1, &y2 );
	if( c == EOF ) return EOF;
	if( c != 4   ) return 0;
	if( x1 >= MAX_WIDTH || x2 >= MAX_WIDTH || y1 >= MAX_HEIGHT || y2 >= MAX_HEIGHT )
		errx( EX_SOFTWARE, "Video frames too big. Increase bounds and recompile." );
	if(
		++p_hist->x[ x1 ] == UINT_MAX ||
		++p_hist->x[ x2 ] == UINT_MAX ||
		++p_hist->y[ y1 ] == UINT_MAX ||
		++p_hist->y[ y2 ] == UINT_MAX
	){
		warnx( "Stopped after maximum number of frames." );
		return 1;
	}
	return 0;
}

void hist_max( unsigned *hist, unsigned hist_sz )
{
	unsigned max = 0;
	for( unsigned i = 0; i < hist_sz; ++i ) {
		if( hist[i] > max ) max = hist[i];
	}
	if( max == 0 ) errx( EX_SOFTWARE, "Cannot read bbox output." );
	hist[ hist_sz ] = max;
}

void print_hist( char *name, unsigned *hist, unsigned hist_sz )
{
	if( save_histogram ) {
		FILE *f = fopen( name, "w" );
		if( !f ) err( EX_CANTCREAT, "%s", name );
		double scale = 100.0 / hist[ hist_sz ];
		for( unsigned i = 0; i < hist_sz; ++i ) {
			for( unsigned j = 0; j < hist[i] * scale; ++j ) putc(( j % 10 ? '-' : '+' ), f );
			putc( '\n', f );
		}
		fclose( f );
	}
	free( name );
}

struct bounds { unsigned first, last; };

#define bounds_len( b ) ((b).last - (b).first + 1)

struct bounds find_bounds( unsigned *hist, unsigned hist_sz )
{
	struct bounds b;
	double th = hist[ hist_sz ] * min_incidence;
	b.first = 0;
	for( unsigned i = b.first; i < hist_sz; ++i ) {
		if( hist[i] >= th ) {
			while( ++i < hist_sz && hist[i] >= th );
			b.first = i - 1;
			break;
		}
	}
	if( b.first % 2 ) ++b.first;
	b.last = hist_sz - 1;
	for( unsigned i = hist_sz; i-- != 0; ) {
		if( hist[i] >= th ) {
			while( i-- != 0 && hist[i] >= th );
			b.last = i + 1;
			break;
		}
	}
	if( b.last % 2 ) ; else --b.last;
	return b;
}

struct bounds round_bounds( struct bounds b, unsigned factor )
{
	unsigned len = bounds_len( b );
	unsigned excess_len = len % factor;
	unsigned offset = excess_len / 2;
	b.first += offset;
	b.last  -= ( excess_len - offset );
	return b;
}

void process_video( void )
{
	static char const cmd_tpl[] = "ffmpeg -v info -hide_banner -ss %u -to %u -i %s -map 0:v:0 -vf bbox=min_val=%u -f null /dev/null 2>&1";
	unsigned duration = video_duration(), start = duration / 12, stop = duration - duration / 6;
	struct bbox_hist hist;
	memset( &hist, 0, sizeof hist );
	process_cmd( alloc_printf( cmd_tpl, start, stop, video_shell_quoted, max_luminance ), parse_bbox, &hist );
	hist_max( hist.x, MAX_WIDTH );
	hist_max( hist.y, MAX_HEIGHT );
	print_hist( alloc_printf( "%s.x_hist", video ), hist.x, MAX_WIDTH );
	print_hist( alloc_printf( "%s.y_hist", video ), hist.y, MAX_HEIGHT );
	struct bounds b_x = find_bounds( hist.x, MAX_WIDTH ), b_y = find_bounds( hist.y, MAX_HEIGHT );
	if( width_factor  ) b_x = round_bounds( b_x, width_factor );
	if( height_factor ) b_y = round_bounds( b_y, height_factor );
	if( crop ) printf( "crop=%u:%u:%u:%u", bounds_len( b_x ), bounds_len( b_y ), b_x.first, b_y.first );
	if( crop && drawbox ) putchar( ' ' );
	if( drawbox ) printf( "drawbox=%u:%u:%u:%u:invert", b_x.first, b_y.first, bounds_len( b_x ), bounds_len( b_y ) );
	if( crop || drawbox ) putchar( '\n' );
}

void* alloc( size_t size )
{
	void *buf = malloc( size );
	if( !buf ) err( EX_UNAVAILABLE, NULL );
	return buf;
}

char* shell_quote( char const *txt )
{
	size_t txt_len = strlen( txt );
	unsigned n_ins = 0;
	for( char const *p = txt, *q = p + txt_len; p != q && ( p = memchr( p, '\'', q - p )); ) {
		n_ins += 3;
		while( ++p != q && *p == '\'' ) ++n_ins;
	}
	char *qtxt = alloc( txt_len + n_ins + 3 ), *w = qtxt;
	*w++ = '\'';
	char const *o = txt, *q = o + txt_len;
	for( char const *p; ( p = memchr( o, '\'', q - o )); ) {
		append_mem( w, o, p - o );
		*w++ = '\'';
		do { append_mem( w, "\\'", 2 ); } while( ++p != q && *p == '\'' );
		*w++ = '\'';
		o = p;
	}
	append_mem( w, o, q - o );
	*w++ = '\'';
	*w = 0;
	return qtxt;
}

void process_cmdline( int argc, char **argv )
{
	for(;;) {
		switch( getopt_long( argc, argv, options, long_options, NULL )) {
			case 'l':
				if( sscanf( optarg, "%u", &max_luminance ) != 1 )
					errx( EX_USAGE, "expected: --max-luminance <unsigned int>" );
				continue;
			case 'i':
				if( sscanf( optarg, "%lf", &min_incidence ) != 1 )
					errx( EX_USAGE, "expected: --min-incidence <float>" );
				continue;
			case 'w':
				if( sscanf( optarg, "%u", &width_factor ) != 1 )
					errx( EX_USAGE, "expected: --width-factor <unsigned int>" );
				continue;
			case 'h':
				if( sscanf( optarg, "%u", &height_factor ) != 1 )
					errx( EX_USAGE, "expected: --height-factor <unsigned int>" );
				continue;
			case 1:
			case 'v':
				video = optarg;
				assign_allocptr( video_shell_quoted, shell_quote( video ));
				process_video();
				continue;
			case 's':
				save_histogram = 1;
				continue;
			case 'c':
				crop = 1;
				continue;
			case 'd':
				drawbox = 1;
				continue;
			case -1:
				return;
		}
	}
}

int main( int argc, char **argv )
{
	process_cmdline( argc, argv );
	return 0;
}
