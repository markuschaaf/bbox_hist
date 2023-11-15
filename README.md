# bbox_hist
Calculates bounding box of a video. Needs `ffmpeg` and `ffprobe`.

## Usage
<pre>
ffmpeg -i <i>source</i> -filter:v $(bbox_hist -c -v <i>source</i>) <i>target</i>
ffmpeg -i <i>source</i> -filter:v $(bbox_hist -d -v <i>source</i>) <i>target</i>
</pre>
The first command crops a video to remove black borders. The second
draws a box instead of cropping, which may help when experimenting
with parameters.

## Options
- --max-luminance *uint*  
  The maximum luminance to consider a pixel black. This is the same as ffmpeg's
  bbox video filter's min_val parameter. Default: 32

- --min-incidence *float*  
  The minimum incidence of a border position in the internal histogram to consider it.
  Expressed as fraction of the maximum incidence. Default: 0.15

- --width-factor *uint*  
  An optional factor the bounding box's width must be divisible by.

- --height-factor *uint*  
  An optional factor the bounding box's height must be divisible by.

- --pre-crop *left*:*right*:*top*:*bottom*  
  Minimum cropping that is applied before measuring the bounding box. Parameters
  state the number of pixel rows or columns, that will be cropped from the left, right,
  top or bottom. Must be positive integers or zero. May help with videos which contain
  static disturbances like white pixels etc.

- --video *filename*  
  The input file. May also be specified as first positional argument.

- --save-histogram  
  Save an ASCII art of the internal histogram of possible border positions.

- --crop  
  Output bounding box as crop parameter suitable for ffmpeg's video filter.

- --drawbox  
  Output bounding box as drawbox parameter suitable for ffmpeg's video filter.
