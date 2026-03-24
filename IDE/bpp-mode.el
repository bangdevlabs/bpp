;;; bpp-mode.el --- Major mode for editing B++ source files.
;;
;; Provides syntax highlighting, indentation, and comment support
;; for .bpp and .bsm files.
;;
;; Installation:
;;   (load "/path/to/b++/bpp-mode.el")
;;   ;; or add to load-path and (require 'bpp-mode)
;;
;; Usage:
;;   Opens automatically for .bpp and .bsm files.
;;   C-c C-c  — compile current file
;;   C-c C-r  — compile and run

(defgroup bpp nil
  "B++ programming language."
  :group 'languages)

(defcustom bpp-compiler "bpp"
  "Path to the B++ compiler."
  :type 'string
  :group 'bpp)

;; Syntax table: C-style // comments and string literals.
(defvar bpp-mode-syntax-table
  (let ((st (make-syntax-table)))
    (modify-syntax-entry ?/ ". 12" st)
    (modify-syntax-entry ?\n ">" st)
    (modify-syntax-entry ?\" "\"" st)
    (modify-syntax-entry ?\' "\"" st)
    (modify-syntax-entry ?_ "w" st)
    st)
  "Syntax table for B++ mode.")

;; Keywords and built-ins for font-lock.
(defvar bpp-font-lock-keywords
  (let* ((keywords '("if" "else" "while" "return" "auto" "import" "struct" "extrn" "sizeof"))
         (builtins '("putchar" "getchar" "malloc" "free" "realloc"
                     "peek" "poke" "str_peek"
                     "argc_get" "argv_get"
                     "sys_write" "sys_read" "sys_open" "sys_close"
                     "sys_fork" "sys_execve" "sys_waitpid" "sys_exit"
                     "fn_ptr" "call"))
         (types '("Vec2" "Style" "Layout" "Watch" "Node"))
         (constants '("TK_KW" "TK_IMP" "TK_ID" "TK_NUM" "TK_HEX" "TK_STR" "TK_OP" "TK_PUNCT" "TK_FLOAT"
                      "T_LIT" "T_VAR" "T_BINOP" "T_UNARY" "T_ASSIGN" "T_MEMLD" "T_MEMST"
                      "T_CALL" "T_IF" "T_WHILE" "T_RET" "T_DECL" "T_NOP"
                      "TY_UNKNOWN" "TY_BYTE" "TY_HALF" "TY_WORD" "TY_LONG" "TY_PTR" "TY_FLOAT"
                      "KEY_UP" "KEY_DOWN" "KEY_LEFT" "KEY_RIGHT"
                      "KEY_SPACE" "KEY_ENTER" "KEY_ESCAPE"
                      "KEY_W" "KEY_A" "KEY_S" "KEY_D"
                      "BLACK" "WHITE" "RED" "GREEN" "BLUE" "YELLOW" "ORANGE" "PURPLE" "GRAY" "DARKGRAY"
                      "SCREEN_W" "SCREEN_H"
                      "LAY_DOWN" "LAY_RIGHT"
                      "MOUSE_LEFT" "MOUSE_RIGHT" "MOUSE_MIDDLE"
                      "NODE_SZ"))
         (kw-re (regexp-opt keywords 'words))
         (bi-re (regexp-opt builtins 'words))
         (ty-re (regexp-opt types 'words))
         (co-re (regexp-opt constants 'words)))
    `((,kw-re . font-lock-keyword-face)
      (,bi-re . font-lock-builtin-face)
      (,ty-re . font-lock-type-face)
      (,co-re . font-lock-constant-face)
      ;; Function definitions: name followed by (.
      ("^\\([a-zA-Z_][a-zA-Z0-9_]*\\)(" 1 font-lock-function-name-face)
      ;; Numeric literals: decimal, hex, char.
      ("\\b0x[0-9a-fA-F]+\\b" . font-lock-constant-face)
      ("\\b[0-9]+\\b" . font-lock-constant-face)
      ("'\\\\?.'" . font-lock-string-face)
      ;; Import paths.
      ("import\\s-+\"\\([^\"]+\\)\"" 1 font-lock-string-face)
      ;; Type annotations after colon.
      (":\\s-*\\([A-Z][a-zA-Z0-9_]*\\)" 1 font-lock-type-face)))
  "Font-lock rules for B++ mode.")

;; Indentation: simple brace-counting.
(defun bpp-indent-line ()
  "Indent current line of B++ code."
  (interactive)
  (let ((indent 0)
        (pos (point)))
    (save-excursion
      (beginning-of-line)
      ;; Count net braces from start of buffer to current line.
      (let ((here (point)))
        (goto-char (point-min))
        (while (< (point) here)
          (cond
           ((looking-at "[ \t]*//") nil)  ; skip comment lines
           ((looking-at "[^{\n]*{") (setq indent (1+ indent)))
           ((looking-at "[ \t]*}") (setq indent (max 0 (1- indent)))))
          (forward-line 1)))
      ;; If current line starts with }, unindent one level.
      (beginning-of-line)
      (when (looking-at "[ \t]*}")
        (setq indent (max 0 (1- indent))))
      ;; Apply indentation.
      (indent-line-to (* 4 indent)))))

;; Compile command.
(defun bpp-compile ()
  "Compile the current B++ file."
  (interactive)
  (let ((file (buffer-file-name)))
    (compile (format "%s %s -o %s"
                     bpp-compiler file
                     (concat (file-name-directory file)
                             "../build/"
                             (file-name-sans-extension (file-name-nondirectory file)))))))

;; Compile and run.
(defun bpp-compile-and-run ()
  "Compile and run the current B++ file."
  (interactive)
  (let* ((file (buffer-file-name))
         (out (concat (file-name-directory file)
                      "../build/"
                      (file-name-sans-extension (file-name-nondirectory file)))))
    (compile (format "%s %s -o %s && %s" bpp-compiler file out out))))

;; Keymap.
(defvar bpp-mode-map
  (let ((map (make-sparse-keymap)))
    (define-key map (kbd "C-c C-c") #'bpp-compile)
    (define-key map (kbd "C-c C-r") #'bpp-compile-and-run)
    map)
  "Keymap for B++ mode.")

;;;###autoload
(define-derived-mode bpp-mode prog-mode "B++"
  "Major mode for editing B++ source code."
  :syntax-table bpp-mode-syntax-table
  (setq-local font-lock-defaults '(bpp-font-lock-keywords))
  (setq-local comment-start "// ")
  (setq-local comment-end "")
  (setq-local indent-line-function #'bpp-indent-line)
  (setq-local tab-width 4))

;;;###autoload
(add-to-list 'auto-mode-alist '("\\.bpp\\'" . bpp-mode))
;;;###autoload
(add-to-list 'auto-mode-alist '("\\.bsm\\'" . bpp-mode))

(provide 'bpp-mode)

;;; bpp-mode.el ends here
