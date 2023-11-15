# bbox_hist
Calculates bounding box of a video. Needs ffmpeg and ffprobe.

## Usage
<pre>
ffmpeg -i <i>source</i> -filter:v $(bbox_hist -c -v <i>source</i>) <i>target</i>
ffmpeg -i <i>source</i> -filter:v $(bbox_hist -d -v <i>source</i>) <i>target</i>
</pre>
The first command crops a video to remove black borders. The second
draws a box instead of cropping, which may help when experimenting
with parameters.

## Options
<dl><dt>--max-luminance <i>unsigned (32)</i></dt>
<dd>The maximum luminance to consider a pixel black. This is the same as ffmpeg's
bbox video filter's min_val parameter.</dd>

<dt>--min-incidence <i>float (0.15)</i></dt>
<dd>The minimum incidence of a border position in the internal histogram to consider it.
Expressed as fraction of the maximum incidence.</dd>

<dt>--width-factor</dt>
<dd>An optional factor the bounding box's width must be divisible by.</dd>

<dt>--height-factor</dt>
<dd>An optional factor the bounding box's height must be divisible by.</dd>

<dt>--pre-crop <i>left:right:top:bottom</i></dt>
<dd>Minimum cropping that is applied before measuring the bounding box. Parameters
state the number of pixel rows or columns, that will be cropped from the left, right,
top or bottom. Must be positive integers or zero. May help with videos which contain
static disturbances like white pixels etc.</dd>

<dt>--video</dt>
<dd>The input file. May also be specified as first positional argument.</dd>

<dt>--save-histogram</dt>
<dd>Save an ASCII art of the internal histogram of possible border positions.</dd>

<dt>--crop</dt>
<dd>Output bounding box as crop parameter suitable for ffmpeg's video filter.</dd>

<dt>--drawbox</dt>
<dd>Output bounding box as drawbox parameter suitable for ffmpeg's video filter.</dd></dl>
