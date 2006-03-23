The FOX-based GUI for imdebug is currently designed to link against the FOX
Toolkit 1.6.0, see http://www.fox-toolkit.org/

By default, the FOX library is statically linked against the C-runtime. As we
use the dynamic runtime in "imdbgdisplay_fox", you need to change the used
runtime to "/MDd" in Debug mode or "/MD" in Release mode in Visual Studio when
compiling FOX to avoid warnings when linking "imdbgdisplay_fox".

NOTE: The file "imdbgicons.h" is generated from image files by the "rewrap"
utility that comes with FOX and is distributed along the image files in the
"res" directory as a binary. In Visual Studio, this is done in the "Pre-Build
Event" of the "imdbgdisplay_fox" project.

Sebastian Schuberth <eyebex@users.berlios.de>, March 2006
