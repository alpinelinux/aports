(ql:quickload :lem-sdl2)

(cffi:close-foreign-library 'async-process::async-process)
(cffi:close-foreign-library 'sdl2::libsdl2)
(cffi:close-foreign-library 'sdl2-ttf::libsdl2-ttf)
(cffi:close-foreign-library 'sdl2-image::libsdl2-image)

(defun setup-foreign-library-directories ()
  (setf cffi:*foreign-library-directories* '())
  (cffi:load-foreign-library "/usr/share/lem/libasyncprocess.so")
  (cffi:load-foreign-library "libdecor-0.so.0")
  (cffi:load-foreign-library "libtiff.so.6")
  (cffi:load-foreign-library "libsamplerate.so.0")
  (cffi:load-foreign-library "libSDL2-2.0.so.0")
  (cffi:load-foreign-library "libSDL2_image-2.0.so.0")
  (cffi:load-foreign-library "libSDL2_ttf-2.0.so.0"))

(load (merge-pathnames "general.lisp" *load-pathname*))

(setf lem-sdl2/resource::*resource-directory* #p"/usr/share/lem/")

(apply #'sb-ext:save-lisp-and-die
       "lem-gui"
       :toplevel 'launch
       :executable t
       #+sb-core-compression
       '(:compression -1)
       #-sb-core-compression
       '())
