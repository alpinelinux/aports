###          freetype2-infinality-ultimate settings          ###
###           rev. 0.4.8.3, for freetype2 v.2.5.x            ###
###                                                          ###
###                Copyright (c) 2014 bohoomil               ###
### The MIT License (MIT) http://opensource.org/licenses/MIT ###
###      part of infinality-bundle  http://bohoomil.com      ###


XFT_SETTINGS="
Xft.antialias:  1
Xft.autohint:   0
Xft.dpi:        96
Xft.hinting:    1
Xft.hintstyle:  hintfull
Xft.lcdfilter:  lcddefault
Xft.rgba:       rgb
"

echo "$XFT_SETTINGS" | xrdb -merge > /dev/null 2>&1

### Available styles:
### 1 <> extra sharp
### 2 <> sharper & lighter ultimate
### 3 <> ultimate: well balanced (default)
### 4 <> darker & smoother
### 5 <> darkest & heaviest ("MacIsh")

USE_STYLE="3"

if [ "$USE_STYLE" = "1" ]; then
  export INFINALITY_FT_FILTER_PARAMS="04 22 38 22 04"
elif [ "$USE_STYLE" = "2" ]; then
  export INFINALITY_FT_FILTER_PARAMS="06 22 36 22 06"
elif [ "$USE_STYLE" = "3" ]; then
  export INFINALITY_FT_FILTER_PARAMS="08 24 36 24 08"
elif [ "$USE_STYLE" = "4" ]; then
  export INFINALITY_FT_FILTER_PARAMS="10 25 37 25 10"
elif [ "$USE_STYLE" = "5" ]; then
  export INFINALITY_FT_FILTER_PARAMS="12 28 42 28 12"
fi

export INFINALITY_FT_FRINGE_FILTER_STRENGTH="25"
export INFINALITY_FT_USE_VARIOUS_TWEAKS="true"
export INFINALITY_FT_WINDOWS_STYLE_SHARPENING_STRENGTH="25"
export INFINALITY_FT_STEM_ALIGNMENT_STRENGTH="15"
export INFINALITY_FT_STEM_FITTING_STRENGTH="15"

# vim:ft=sh:
