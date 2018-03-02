(add-to-list 'load-path "/usr/share/emacs/site-lisp/emacs-ycmd/")

(setq debug-on-error t)

(add-to-list 'load-path "/usr/share/emacs/site-lisp/emacs-ycmd/contrib")
(require 'ycmd-next-error)

(set-variable 'ycmd-server-command '("python3" "/usr/lib/python3.6/site-packages/ycmd/ycmd"))
(set-variable 'ycmd-global-config "/usr/lib/python3.6/site-packages/ycmd/.ycm_extra_conf.py")
(set-variable 'ycmd-gocode-binary-path "/usr/lib/python3.6/site-packages/ycmd/third_party/gocode/gocode")
(set-variable 'ycmd-godef-binary-path "/usr/lib/python3.6/site-packages/ycmd/third_party/godef/godef")
(set-variable 'ycmd-rust-src-path "/usr/src/rust")
(set-variable 'ycmd-racerd-binary-path "/usr/lib/python3.6/site-packages/ycmd/third_party/racerd/target/release/racerd")
(set-variable 'ycmd-python-binary-path "/usr/bin/python3")

(require 'ycmd)
(add-hook 'after-init-hook #'global-ycmd-mode)

(defun ycmd-setup-completion-at-point-function ()
  "Setup \`completion-at-point-functions' for \`ycmd-mode'."
  (add-hook 'completion-at-point-functions
            #'ycmd-complete-at-point nil :local))

(add-hook 'ycmd-mode #'ycmd-setup-completion-at-point-function)

(when (require 'company nil t)
  (add-hook 'after-init-hook 'global-company-mode))
(when (require 'elpy nil t)
(elpy-enable))

(require 'company-ycmd)
(company-ycmd-setup)
(add-hook 'after-init-hook 'global-company-mode)

(require 'flycheck-ycmd)
(flycheck-ycmd-setup)

(require 'ycmd-eldoc)
(add-hook 'ycmd-mode-hook 'ycmd-eldoc-setup)
