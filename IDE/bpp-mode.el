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

(defvar bpp-font-lock-keywords
  (let* (
         ;; Control flow keywords.
         (control '("if" "else" "while" "for" "return" "break" "continue" "switch"))

         ;; Declaration and storage class keywords.
         ;; static: file-scope private (not exported).
         ;; global: worker-shared mutable.
         ;; extrn:  write-once-after-init, safe to read from any worker.
         ;; void:   no return value.
         (decl '("auto" "struct" "enum" "extrn" "global" "static" "const" "void"
                 "import" "sizeof"))

         ;; Compiler builtins — intercepts handled before codegen.
         (builtins '(;; Memory primitives
                     "malloc" "free" "realloc" "memcpy" "peek" "poke"
                     ;; IO dispatch (put dispatches by type: TY_PTR→putstr, TY_FLOAT→putfloat, TY_WORD→putnum)
                     "put" "put_err" "putchar" "putchar_err" "getchar" "str_peek" "envp_get"
                     ;; Meta builtins
                     "fn_ptr" "call" "assert" "shr" "argc_get" "argv_get"
                     "mem_barrier" "float_ret" "float_ret2"
                     ;; Syscalls
                     "sys_read" "sys_write" "sys_open" "sys_close" "sys_exit"
                     "sys_fork" "sys_execve" "sys_waitpid"
                     "sys_socket" "sys_connect" "sys_ioctl"
                     "sys_lseek" "sys_fchmod" "sys_getdents"
                     "sys_mkdir" "sys_unlink" "sys_ptrace"
                     "sys_nanosleep" "sys_usleep" "sys_clock_gettime"))

         ;; Named constants: AST nodes, token kinds, type codes, dispatch codes.
         (constants '(;; AST node type codes
                      "T_LIT" "T_VAR" "T_BINOP" "T_UNARY" "T_ASSIGN"
                      "T_MEMLD" "T_MEMST" "T_CALL" "T_IF" "T_WHILE"
                      "T_RET" "T_DECL" "T_NOP" "T_BREAK" "T_ADDR"
                      "T_CONTINUE" "T_SWITCH" "T_TERNARY" "T_BLOCK"
                      ;; Token kind codes
                      "TK_KW" "TK_IMP" "TK_ID" "TK_NUM" "TK_HEX"
                      "TK_STR" "TK_OP" "TK_PUNCT" "TK_FLOAT"
                      ;; Type codes (current naming — base × slice encoding)
                      "TY_UNK"
                      "TY_WORD" "TY_WORD_H" "TY_WORD_Q" "TY_WORD_B"
                      "TY_WORD_BIT" "TY_WORD_BIT2" "TY_WORD_BIT3" "TY_WORD_BIT4"
                      "TY_WORD_BIT5" "TY_WORD_BIT6" "TY_WORD_BIT7"
                      "TY_FLOAT" "TY_FLOAT_H" "TY_FLOAT_Q" "TY_FLOAT_B" "TY_FLOAT_D"
                      "TY_PTR" "TY_ARR" "TY_STRUCT"
                      ;; Base and slice constants
                      "BASE_WORD" "BASE_FLOAT" "BASE_PTR" "BASE_ARR" "BASE_STRUCT" "BASE_UNKNOWN"
                      "SL_FULL" "SL_HALF" "SL_QUARTER" "SL_BYTE"
                      "SL_BIT" "SL_DOUBLE"
                      ;; Dispatch codes
                      "DSP_SEQ" "DSP_PAR" "DSP_GPU" "DSP_SPLIT"
                      ;; Phase/effect codes
                      "PHASE_AUTO" "PHASE_BASE" "PHASE_SOLO"
                      "PHASE_REALTIME" "PHASE_IO" "PHASE_GPU" "PHASE_HEAP" "PHASE_PANIC"
                      ;; Global storage class codes
                      "GLOB_AUTO" "GLOB_GLOBAL" "GLOB_EXTRN"
                      ;; Node layout offsets
                      "NODE_SZ" "RECORD_SZ" "ND_TYPE" "ND_ITYPE"
                      "FN_TYPE" "FN_NAME" "FN_PARS" "FN_PCNT"
                      "FN_BODY" "FN_BCNT" "FN_HINTS" "FN_STUB" "FN_SRC_TOK"
                      "EX_TYPE" "EX_NAME" "EX_LIB" "EX_RET" "EX_ARGS" "EX_ACNT"
                      ;; Input keys
                      "KEY_UP" "KEY_DOWN" "KEY_LEFT" "KEY_RIGHT"
                      "KEY_W" "KEY_A" "KEY_S" "KEY_D"
                      "KEY_SPACE" "KEY_ENTER" "KEY_ESC" "KEY_TAB" "KEY_SHIFT"
                      "KEY_E" "KEY_R" "KEY_Z" "KEY_X" "KEY_G" "KEY_C" "KEY_N" "KEY_T"
                      "KEY_0" "KEY_1" "KEY_2" "KEY_3" "KEY_4" "KEY_5"
                      "KEY_6" "KEY_7" "KEY_8" "KEY_9"
                      "KEY_F5" "KEY_F6" "KEY_F9" "KEY_F10"
                      "KEY_PERIOD" "KEY_BACKSPACE"
                      "MOUSE_LEFT" "MOUSE_RIGHT" "MOUSE_MIDDLE"
                      ;; Colors
                      "BLACK" "WHITE" "RED" "GREEN" "BLUE" "YELLOW"
                      "ORANGE" "PURPLE" "GRAY" "DARKGRAY"
                      ;; Null sentinel
                      "NULL"))

         (ctrl-re  (regexp-opt control   'words))
         (decl-re  (regexp-opt decl      'words))
         (bi-re    (regexp-opt builtins  'words))
         (co-re    (regexp-opt constants 'words)))

    `(;; Keywords first so they take priority over function-name matches.
      (,ctrl-re  . font-lock-keyword-face)
      (,decl-re  . font-lock-keyword-face)
      (,bi-re    . font-lock-builtin-face)
      (,co-re    . font-lock-constant-face)

      ;; Function definitions. Handles all prefix combinations:
      ;;   static void fn(   static fn(   void fn(   fn(
      ;; The \\(?: ... \\) groups are non-capturing; group 1 = function name.
      ("^\\(?:static\\s-+\\)?\\(?:void\\s-+\\)?\\([a-zA-Z_][a-zA-Z0-9_]*\\)\\s-*("
       1 font-lock-function-name-face)

      ;; Effect / phase annotations after colon: : base, : solo, : io, etc.
      (":\\s-*\\(base\\|solo\\|io\\|realtime\\|gpu\\|heap\\)\\b"
       1 font-lock-preprocessor-face)

      ;; Type annotations after colon: : word, : float, : array, : ptr,
      ;; and slice variants: : byte word, : half float, etc.
      (":\\s-*\\(?:\\(half\\|quarter\\|byte\\)\\s-+\\)?\\(word\\|float\\|ptr\\|array\\)\\b"
       (1 font-lock-type-face nil t)
       (2 font-lock-type-face))

      ;; Numeric literals.
      ("\\b0x[0-9a-fA-F]+\\b"  . font-lock-constant-face)
      ("\\b[0-9]+\\.[0-9]+\\b" . font-lock-constant-face)
      ("\\b[0-9]+\\b"          . font-lock-constant-face)
      ("'\\\\?.'"              . font-lock-string-face)

      ;; Import paths (the path string after import).
      ("\\bimport\\s-+\"\\([^\"]+\\)\"" 1 font-lock-string-face)))

  "Font-lock rules for bpp-mode.")

;; Indentation: simple brace-counting.
(defun bpp-indent-line ()
  "Indent current line of B++ code."
  (interactive)
  (let ((indent 0))
    (save-excursion
      (beginning-of-line)
      (let ((here (point)))
        (goto-char (point-min))
        (while (< (point) here)
          (cond
           ((looking-at "[ \t]*//") nil)
           ((looking-at "[^{\n]*{") (setq indent (1+ indent)))
           ((looking-at "[ \t]*}") (setq indent (max 0 (1- indent)))))
          (forward-line 1)))
      (beginning-of-line)
      (when (looking-at "[ \t]*}")
        (setq indent (max 0 (1- indent))))
      (indent-line-to (* 4 indent)))))

;; Compile commands.
(defun bpp-compile ()
  "Compile the current B++ file."
  (interactive)
  (let ((file (buffer-file-name)))
    (compile (format "%s %s -o %s"
                     bpp-compiler file
                     (concat (file-name-directory file)
                             "../build/"
                             (file-name-sans-extension
                              (file-name-nondirectory file)))))))

(defun bpp-compile-and-run ()
  "Compile and run the current B++ file."
  (interactive)
  (let* ((file (buffer-file-name))
         (out (concat (file-name-directory file)
                      "../build/"
                      (file-name-sans-extension
                       (file-name-nondirectory file)))))
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
