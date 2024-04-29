(ql:quickload :lem-ncurses)

(cffi:close-foreign-library 'async-process::async-process)

(defun setup-foreign-library-directories ()
  (setf cffi:*foreign-library-directories* '())
  (cffi:load-foreign-library "/usr/share/lem/libasyncprocess.so"))

(load (merge-pathnames "general.lisp" *load-pathname*))

(apply #'sb-ext:save-lisp-and-die
       "lem"
       :toplevel 'launch
       :executable t
       #+sb-core-compression
       '(:compression -1)
       #-sb-core-compression
       '())
