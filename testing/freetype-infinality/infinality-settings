##################################################################
# INFINALITY ENVIRONMENT VARIABLES FOR EXTRA RUN-TIME OPTIONS
##################################################################
#
# These environment variables require that their respective patches
# from http://www.infinality.net have been applied to the Freetype
# installation you are using.  They will do abolutely
# nothing otherwise!
#

# This file should be copied to /etc/profile.d/ for system-wide
# effects and/or included in ~/.bashrc or ~/.bash_profile for per-user
# effects:
# 
#   . ~/path/to/this/file/infinality-settings
#
# Of course, the per-user settings will override the system-wide
# settings.  Default values indicated below will be used when the
# environment variables below are not defined.



##################################################################
# INFINALITY_FT_FILTER_PARAMS
#
# This is a modified version of the patch here:
# http://levelsofdetail.kendeeter.com/2008/12/dynamic_fir_filter_patch.html
#
# Allows you to adjust the FIR filter at runtime instead of at
# compile time.  The idea is to have values add up to one, and be
# symmetrical around the middle value.  Here are some samples
# of various filter parameters:
#
# Strong Extra Smooth  "15e-2 20e-2 30e-2 20e-2 15e-2"  (extra smooth, natural weight)
# Extra Smooth      "20e-2 20e-2 30e-2 20e-2 20e-2"  (extra smooth, extra weight)
# Smooth            "15e-2 20e-2 32e-2 20e-2 15e-2"  (smooth, natural weight)
# Stronger Gibson   "11e-2 22e-2 38e-2 22e-2 11e-2"  (smooth, extra weight)
# Gibson            "11e-2 22e-2 33e-2 22e-2 11e-2"  (smooth, natural weight)
# Freetype Light    "00e-2 33e-2 34e-2 33e-2 00e-2"  (sharp, natural weight)
# Freetype Default  "06e-2 25e-2 44e-2 25e-2 06e-2"  (sharp, extra weight)           *default
# Extra Sharp       "00e-2 35e-2 35e-2 35e-2 00e-2"  (extra sharp, extra weight)
#
# Default:     [Freetype's default]
# Recommended: "11e-2 22e-2 38e-2 22e-2 11e-2"
#
# Example 1:  export INFINALITY_FT_FILTER_PARAMS="11e-2 22e-2 38e-2 22e-2 11e-2"
#

export INFINALITY_FT_FILTER_PARAMS="11e-2 22e-2 38e-2 22e-2 11e-2"




##################################################################
# INFINALITY_FT_STEM_ALIGNMENT_TYPE
#
# This performs analysis on each glyph and determines the best
# subpixel orientation for the glyph.  The glyph is not scaled in
# any way, just moved left or right by a subpixel amount.  This
# results in subtley cleaner looking fonts, at the expense of
# proper distances between glyphs.  This is only active for sizes
# 10 px or greater and does not apply to bold or italic fonts.
# 
# Possible values:
# full             - Allows a glyph to be moved to the LEFT or RIGHT by 1 subpixel
#                    Best alignment, Worst positioning
# medium,medium1   - Only allows a glyph to be moved to the LEFT by 1 subpixel
#                    Good alignment, Good positioning
# medium2          - Only allows a glyph to be moved to the RIGHT by 1 subpixel
#                    Good alignment, Good positioning
# slight,slight1   - A stricter version of medium
#                    Minor alignment, Best positioning
# slight2          - A stricter version of medium2
#                    Minor alignment, Best positioning
# infinality       - medium1 when stem < 5 subpixels, full when >= 5 subpixels
# none             - Don't do any alignment
#
# Default:     none
# Recommended: medium

export INFINALITY_FT_STEM_ALIGNMENT_TYPE=medium




##################################################################
# INFINALITY_FT_AUTOFIT_STEM_SNAP_LIGHT
#
# Cause the height of horizontal stems to snap to integer pixels
# when using light auto-hinting.  (This happens automatically
# when using full auto-hinting)
#
# This produces an effect similar to the way Windows renders fonts
# without requiring the font to contain bytecode instructions.
#
# Possible values:
# true             - enable stem snapping
# false            - do not enable stem snapping
#
# Default:     false
# Recommended: true

export INFINALITY_FT_AUTOFIT_STEM_SNAP_LIGHT=true




##################################################################
# INFINALITY_FT_AUTOFIT_EMBOLDEN_LIGHT
#
# Embolden particularly light or thin fonts, like DejaVu Sans Light,
# Inconsolata, Freemono, Courier New, etc. up until stem width is
# 1 pixel wide.  This makes these fonts easier to read at lower
# ppems.  Only applies when the autohinter is being used.
#
# Possible values:
# true             - enable emboldening of light fonts
# false            - do not enable emboldening of light fonts
#
# Default:     false
# Recommended: true

export INFINALITY_FT_AUTOFIT_EMBOLDEN_LIGHT=true




##################################################################
# INFINALITY_FT_PSEUDO_GAMMA
#
# This does a weighted gamma correction at the LCD filter phase
# prior to the LCD filter.
#
# The first value indicates a px value, the second indicates a
# "gamma" value.  All sizes < the px value will be corrected
# on a weighted scale based on the second value.
#
# Values .1 < 1.0 will darken the glyph
# Values > 1.0 will lighten the glyph
#
# Example 1:  Darken glyphs that are less than 10 px. With some fonts
#             even 5 or 6px is readable!
# export INFINALITY_FT_PSEUDO_GAMMA="10 6e-1"
#
# Example 2:  Lighten all glyphs (below 100px)
# export INFINALITY_FT_PSEUDO_GAMMA="100 15e-1"
#
# Default:     [No gamma correction]
# Recommended: "9 5e-1"

export INFINALITY_FT_PSEUDO_GAMMA="9 7e-1"




##################################################################
# INFINALITY_FT_AUTOFIT_ADJUST_HEIGHTS
#
# This will slightly stretch some glyphs vertically between 9px
# and 14px (inclusive).  Some people may find this more
# aesthetically pleasing.  This only applies to fonts that are
# using autohint.  
#
# Possible values:
# true             - enable height adjustment
# false            - do not enable height adjustment
#
# Default:     false

export INFINALITY_FT_AUTOFIT_ADJUST_HEIGHTS=true




##################################################################
# INFINALITY_FT_ENHANCED_EMBOLDEN
#
# When doing artificial emboldening, only embolden in the X
# direction, skipping the Y direction. Most people will find this
# more aesthetically pleasing than the default behavior.
#
# Possible values:
# true             - enable enhanced emboldening
# false            - no not enable enhanced emboldening
#
# Default:     false
# Recommended: true

export INFINALITY_FT_ENHANCED_EMBOLDEN=true




##################################################################
# INFINALITY_FT_EMBOLDEN_MAINTAIN_WIDTH
#
# When doing artificial emboldening, don't change the glyph width.
#
# Possible values:
# true             - maintain width
# false            - do not maintain width
#
# Default:     false
# Recommended: true

export INFINALITY_FT_EMBOLDEN_MAINTAIN_WIDTH=true




##################################################################
# INFINALITY_FT_AUTO_AUTOHINT
#
# Automatically use autohint when rendering a font that contains
# no truetype instructions, regardless of what the calling
# program asks for.  The truetype hinter will not do a good job
# on these.
#
# Possible values:
# true             - automatically use autohint
# false            - do not automatically use autohint
#
# Default:     false
# Recommended: true

export INFINALITY_FT_AUTO_AUTOHINT=true




