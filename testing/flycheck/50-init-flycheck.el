(add-to-list 'load-path "/usr/share/emacs/site-lisp/flycheck/")
(require 'flycheck)
(add-hook 'after-init-hook #'global-flycheck-mode)
