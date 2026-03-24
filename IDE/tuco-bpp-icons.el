;;; tuco-bpp-icons.el --- B++ icons for Treemacs -*- lexical-binding: t; -*-
;;
;; Floppy disk icon for .bpp (program files)
;; IC chip icon for .bsm (module files)
;;
;; Setup:
;;   1. Copy bpp-icon.png, bpp-icon@2x.png, bsm-icon.png, bsm-icon@2x.png
;;      into ~/.emacs.d/bpp-icons/
;;   2. Add (require 'tuco-bpp-icons) to your init, or load this file directly.

(defvar tuco-bpp-icons-dir
  (expand-file-name "bpp-icons" user-emacs-directory)
  "Directory containing B++ icon PNGs for Treemacs.")

(with-eval-after-load 'treemacs
  ;; .bpp — floppy disk (program / executable source)
  (treemacs-define-custom-icon
   (concat " "
           (treemacs--create-image
            (expand-file-name "bpp-icon.png" tuco-bpp-icons-dir)
            16 16))
   "bpp")

  ;; .bsm — IC chip (B-Single-Module / library)
  (treemacs-define-custom-icon
   (concat " "
           (treemacs--create-image
            (expand-file-name "bsm-icon.png" tuco-bpp-icons-dir)
            16 16))
   "bsm"))

(provide 'tuco-bpp-icons)
;;; tuco-bpp-icons.el ends here
