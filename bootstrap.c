#include <stdint.h>
#include <unistd.h>
#include <sys/wait.h>


#include <stdlib.h>
#include <string.h>
static uint8_t _bpp_scratch[16];
static int _bpp_argc;
static char** _bpp_argv;

// B++ I/O runtime — raw syscalls.
static long long bpp_sys_write(long long fd, const void* buf, long long len) {
  register long long x0 __asm__("x0") = fd;
  register long long x1 __asm__("x1") = (long long)buf;
  register long long x2 __asm__("x2") = len;
  register long long x16 __asm__("x16") = 4;
  __asm__ volatile("svc #0x80\n\tcsneg x0, x0, x0, cc" : "+r"(x0) : "r"(x1), "r"(x2), "r"(x16) : "memory", "cc");
  return x0;
}
static long long bpp_sys_read(long long fd, void* buf, long long len) {
  register long long x0 __asm__("x0") = fd;
  register long long x1 __asm__("x1") = (long long)buf;
  register long long x2 __asm__("x2") = len;
  register long long x16 __asm__("x16") = 3;
  __asm__ volatile("svc #0x80\n\tcsneg x0, x0, x0, cc" : "+r"(x0) : "r"(x1), "r"(x2), "r"(x16) : "memory", "cc");
  return x0;
}
static long long bpp_sys_open(const char* path, long long flags) {
  register long long x0 __asm__("x0") = (long long)path;
  register long long x1 __asm__("x1") = flags;
  register long long x2 __asm__("x2") = 493;
  register long long x16 __asm__("x16") = 5;
  __asm__ volatile("svc #0x80\n\tcsneg x0, x0, x0, cc" : "+r"(x0) : "r"(x1), "r"(x2), "r"(x16) : "memory", "cc");
  return x0;
}
static long long bpp_sys_close(long long fd) {
  register long long x0 __asm__("x0") = fd;
  register long long x16 __asm__("x16") = 6;
  __asm__ volatile("svc #0x80\n\tcsneg x0, x0, x0, cc" : "+r"(x0) : "r"(x16) : "memory", "cc");
  return x0;
}
static long long bpp_sys_fork(void) { return (long long)fork(); }
static long long bpp_sys_execve(long long p, long long a, long long e) {
  return (long long)execve((const char*)(uintptr_t)p,(char*const*)(uintptr_t)a,(char*const*)(uintptr_t)e); }
static long long bpp_sys_waitpid(long long pid) { int s; waitpid((pid_t)pid,&s,0); return 0; }
static long long bpp_sys_exit(long long c) { _exit((int)c); return 0; }

long long T_LIT = 0;
long long T_VAR = 0;
long long T_BINOP = 0;
long long T_UNARY = 0;
long long T_ASSIGN = 0;
long long T_MEMLD = 0;
long long T_MEMST = 0;
long long T_CALL = 0;
long long T_IF = 0;
long long T_WHILE = 0;
long long T_RET = 0;
long long T_DECL = 0;
long long T_NOP = 0;
long long T_BREAK = 0;
long long TK_KW = 0;
long long TK_IMP = 0;
long long TK_ID = 0;
long long TK_NUM = 0;
long long TK_HEX = 0;
long long TK_STR = 0;
long long TK_OP = 0;
long long TK_PUNCT = 0;
long long TY_UNKNOWN = 0;
long long TY_BYTE = 0;
long long TY_QUART = 0;
long long TY_HALF = 0;
long long TY_WORD = 0;
long long TY_PTR = 0;
long long TY_FLOAT = 0;
long long TY_STRUCT = 0;
long long NODE_SZ = 0;
long long ND_TYPE = 0;
long long ND_ITYPE = 0;
long long FN_TYPE = 0;
long long FN_NAME = 0;
long long FN_PARS = 0;
long long FN_PCNT = 0;
long long FN_BODY = 0;
long long FN_BCNT = 0;
long long FN_HINTS = 0;
long long EX_TYPE = 0;
long long EX_NAME = 0;
long long EX_LIB = 0;
long long EX_RET = 0;
long long EX_ARGS = 0;
long long EX_ACNT = 0;
long long _ARR_HDR = 0;
long long _ARR_ELEM = 0;
long long _ARR_INIT = 0;
long long tmp_arr = 0;
long long list_stack = 0;
long long list_sp = 0;
long long list_depth = 0;
long long last_list_cnt = 0;
long long _diag_buf = 0;
long long _diag_warn_cnt = 0;
long long fbuf = 0;
long long fbuf_top = 0;
long long imp_hashes = 0;
long long pathbuf = 0;
long long linebuf = 0;
long long outbuf = 0;
long long outbuf_len = 0;
long long fname_buf = 0;
long long fname_pos = 0;
long long diag_file_starts = 0;
long long diag_file_names = 0;
long long diag_file_line_starts = 0;
long long outbuf_lines = 0;
long long mod_hashes = 0;
long long dep_from = 0;
long long dep_to = 0;
long long mod_topo = 0;
long long mod_stale = 0;
long long _cache_buf = 0;
long long _imp_target = 0;
long long _imp_target_len = 0;
long long toks = 0;
long long tok_cnt = 0;
long long tok_pos = 0;
long long vbuf = 0;
long long vbuf_pos = 0;
long long globals = 0;
long long glob_cnt = 0;
long long externs = 0;
long long ext_cnt = 0;
long long funcs = 0;
long long func_cnt = 0;
long long sd_names = 0;
long long sd_fields = 0;
long long sd_fcounts = 0;
long long sd_sizes = 0;
long long vt_names = 0;
long long vt_sidx = 0;
long long var_stack_names = 0;
long long ev_names = 0;
long long ev_values = 0;
long long TK_FLOAT = 0;
long long _prim_hint = 0;
long long func_mods = 0;
long long glob_mods = 0;
long long struct_mods = 0;
long long enum_mods = 0;
long long extern_mods = 0;
long long src = 0;
long long src_len = 0;
long long pos = 0;
long long tok_lines = 0;
long long cur_line = 0;
long long tok_src_off = 0;
long long ty_vt_names = 0;
long long ty_vt_types = 0;
long long ft_names = 0;
long long ft_types = 0;
long long fn_vt_data = 0;
long long fn_vt_cnts = 0;
long long fn_ret_types = 0;
long long fn_par_types = 0;
long long ty_cur_ret = 0;
long long ty_gl_names = 0;
long long ty_gl_types = 0;
long long DSP_SEQ = 0;
long long DSP_PAR = 0;
long long DSP_GPU = 0;
long long DSP_SPLIT = 0;
long long pure_ext = 0;
long long stride_vars = 0;
long long dsp_local_vars = 0;
long long sbuf = 0;
long long ex_name = 0;
long long ex_lib = 0;
long long ex_ret = 0;
long long ex_args = 0;
long long ex_acnt = 0;
long long ex_cnt = 0;
long long fn_name = 0;
long long fn_par = 0;
long long fn_pcnt = 0;
long long fn_body = 0;
long long fn_bcnt = 0;
long long fn_cnt = 0;
long long fn_fidx = 0;
long long gl_name = 0;
long long gl_cnt = 0;
long long ux_name = 0;
long long ux_cnt = 0;
long long g_indent = 0;
long long cur_fn_idx = 0;
long long enc_buf = 0;
long long enc_pos = 0;
long long enc_lbl_off = 0;
long long enc_lbl_max = 0;
long long enc_fix_pos = 0;
long long enc_fix_lbl = 0;
long long enc_fix_ty = 0;
long long enc_fix_cnt = 0;
long long enc_rel_pos = 0;
long long enc_rel_sym = 0;
long long enc_rel_ty = 0;
long long enc_rel_cnt = 0;
long long a64_sbuf = 0;
long long a64_fn_name = 0;
long long a64_fn_par = 0;
long long a64_fn_pcnt = 0;
long long a64_fn_body = 0;
long long a64_fn_bcnt = 0;
long long a64_fn_fidx = 0;
long long a64_gl_name = 0;
long long a64_ex_name = 0;
long long a64_ex_ret = 0;
long long a64_ex_args = 0;
long long a64_ex_acnt = 0;
long long a64_vars = 0;
long long a64_var_stack = 0;
long long a64_var_struct_idx = 0;
long long a64_var_off = 0;
long long a64_var_forced_ty = 0;
long long a64_flt_tbl = 0;
long long a64_str_tbl = 0;
long long a64_lbl_cnt = 0;
long long a64_depth = 0;
long long a64_break_stack = 0;
long long a64_cur_fn_name = 0;
long long a64_cur_fn_idx = 0;
long long a64_bin_mode = 0;
long long a64_fn_lbl = 0;
long long a64_ret_lbl = 0;
long long a64_argc_sym = 0;
long long a64_argv_sym = 0;
long long MO_PAGE_SIZE = 0;
long long MO_VM_BASE = 0;
long long MO_VM_BASE_HI = 0;
long long mo_buf = 0;
long long mo_pos = 0;
long long mo_ncmds = 0;
long long mo_sizeofcmds = 0;
long long mo_code_off = 0;
long long mo_text_vmsize = 0;
long long mo_data_off = 0;
long long mo_data_size = 0;
long long mo_data_vmsize = 0;
long long mo_link_off = 0;
long long mo_link_size = 0;
long long mo_link_vmsize = 0;
long long mo_main_off = 0;
long long mo_data_vmaddr_lo = 0;
long long mo_data_vmaddr_hi = 0;
long long mo_link_vmaddr_lo = 0;
long long mo_link_vmaddr_hi = 0;
long long mo_cs_off = 0;
long long mo_cs_size = 0;
long long mo_cf_off = 0;
long long mo_cf_size = 0;
long long mo_et_off = 0;
long long mo_et_size = 0;
long long mo_sym_off = 0;
long long mo_sym_cnt = 0;
long long mo_str_off = 0;
long long mo_str_size = 0;
long long mo_flt_data = 0;
long long mo_sdata = 0;
long long mo_sdata_size = 0;
long long mo_sdata_off = 0;
long long mo_got_syms = 0;
long long mo_got_libs = 0;
long long mo_got_data_off = 0;
long long mo_ext_libs = 0;
long long sha_k = 0;
long long sha_w = 0;
long long x64_enc_buf = 0;
long long x64_enc_pos = 0;
long long x64_enc_lbl_off = 0;
long long x64_enc_fix_pos = 0;
long long x64_enc_fix_lbl = 0;
long long x64_enc_rel_pos = 0;
long long x64_enc_rel_sym = 0;
long long x64_enc_rel_ty = 0;
long long CC_O = 0;
long long CC_NO = 0;
long long CC_B = 0;
long long CC_AE = 0;
long long CC_E = 0;
long long CC_NE = 0;
long long CC_BE = 0;
long long CC_A = 0;
long long CC_S = 0;
long long CC_NS = 0;
long long CC_P = 0;
long long CC_NP = 0;
long long CC_L = 0;
long long CC_GE = 0;
long long CC_LE = 0;
long long CC_G = 0;
long long x64_sbuf = 0;
long long x64_fn_name = 0;
long long x64_fn_par = 0;
long long x64_fn_pcnt = 0;
long long x64_fn_body = 0;
long long x64_fn_bcnt = 0;
long long x64_fn_fidx = 0;
long long x64_gl_name = 0;
long long x64_ex_name = 0;
long long x64_ex_ret = 0;
long long x64_ex_args = 0;
long long x64_ex_acnt = 0;
long long x64_vars = 0;
long long x64_var_stack = 0;
long long x64_var_struct_idx = 0;
long long x64_var_off = 0;
long long x64_var_forced_ty = 0;
long long x64_flt_tbl = 0;
long long x64_str_tbl = 0;
long long x64_lbl_cnt = 0;
long long x64_depth = 0;
long long x64_break_stack = 0;
long long x64_cur_fn_name = 0;
long long x64_cur_fn_idx = 0;
long long x64_fn_lbl = 0;
long long x64_ret_lbl = 0;
long long x64_argc_sym = 0;
long long x64_argv_sym = 0;
long long ELF_PAGE = 0;
long long ELF_BASE = 0;
long long ELF_HDRSZ = 0;
long long ELF_PHSZ = 0;
long long elf_buf = 0;
long long elf_pos = 0;
long long elf_code_off = 0;
long long elf_text_filesz = 0;
long long elf_text_memsz = 0;
long long elf_data_off = 0;
long long elf_data_vaddr = 0;
long long elf_data_filesz = 0;
long long elf_data_memsz = 0;
long long elf_entry = 0;
long long elf_phnum = 0;
long long elf_has_data = 0;
long long elf_flt_data = 0;
long long elf_sdata = 0;
long long elf_sdata_size = 0;
long long elf_sdata_off = 0;
long long bo_buf = 0;
long long bo_pos = 0;
long long bo_size = 0;
long long bo_pathbuf = 0;

long long pack(long long s, long long l);
long long unpack_s(long long p);
long long unpack_l(long long p);
uint8_t init_defs(void);
uint8_t init_array(void);
long long arr_new(void);
long long arr_grow(long long arr, long long new_cap);
long long arr_push(long long arr, long long val);
long long arr_pop(long long arr);
long long arr_len(long long arr);
long long arr_cap(long long arr);
long long arr_get(long long arr, long long i);
uint8_t arr_set(long long arr, long long i, long long val);
long long arr_last(long long arr);
uint8_t arr_clear(long long arr);
uint8_t arr_free(long long arr);
uint8_t init_internal(void);
uint8_t out(long long s);
uint8_t buf_eq(long long a, long long b, long long len);
uint8_t print_buf(long long start, long long len);
uint8_t packed_eq(long long p1, long long p2);
long long make_node(long long type);
uint8_t list_begin(void);
uint8_t list_push(long long val);
long long list_end(void);
uint8_t init_diag(void);
uint8_t diag_ch(long long c);
uint8_t diag_str(long long s);
uint8_t diag_int(long long n);
uint8_t diag_packed(long long p);
uint8_t diag_buf(long long addr, long long len);
uint8_t diag_newline(void);
uint8_t diag_fatal(long long code);
uint8_t diag_warn(long long code);
uint8_t diag_loc(long long tok_idx);
uint8_t diag_show_line(long long tok_idx);
uint8_t diag_help(long long msg);
uint8_t diag_summary(void);
uint8_t emit_ch(long long ch);
uint8_t emit_str(long long s);
uint8_t copy_cstr(long long dest, long long src);
long long hash_mem(long long off, long long len);
uint8_t last_slash(long long off, long long len);
uint8_t was_imported(long long path_off, long long path_len);
uint8_t mark_imported(long long path_off, long long path_len);
long long mod_idx(long long src_off);
uint8_t mod_idx_by_hash(long long path_off, long long path_len);
long long fnv1a(long long off, long long len);
uint8_t topo_sort(void);
long long hex_emit(long long buf, long long off, long long val);
long long hex_parse(long long buf, long long off);
long long cache_manifest_hash(void);
uint8_t cache_load(void);
uint8_t cache_save(void);
uint8_t propagate_stale(void);
uint8_t resolve_path(long long base_off, long long base_len, long long name_off, long long name_len);
uint8_t init_target(void);
long long find_file(long long base_off, long long base_len, long long name_off, long long name_len);
long long check_file_import(long long loff, long long llen);
uint8_t process_file(long long path_off, long long path_len);
uint8_t init_import(void);
uint8_t init_parser(void);
long long add_struct_def(long long name_p);
uint8_t add_struct_field(long long idx, long long field_p);
uint8_t find_struct(long long name_p);
uint8_t get_field_offset(long long idx, long long field_p);
long long get_struct_size(long long idx);
uint8_t set_var_type(long long var_p, long long idx);
long long get_var_type(long long var_p);
uint8_t set_var_stack(long long var_p);
uint8_t is_var_stack(long long var_p);
uint8_t add_enum_val(long long name_p, long long val);
long long find_enum_val(long long name_p);
long long parse_lit_int(void);
long long make_int_lit(long long val);
long long make_plus_op(void);
long long peek_type(void);
long long peek_type_at(long long off);
long long peek_vstart(void);
long long peek_vlen(void);
long long cur_packed(void);
long long cur_type(void);
uint8_t consume(void);
long long val_is_ch(long long c);
uint8_t val_is(long long s, long long slen);
long long val_at_ch(long long off, long long c);
long long tok_mod(long long tidx);
uint8_t add_global(long long p);
uint8_t try_type_annotation(long long var_p);
uint8_t parse_global(void);
uint8_t parse_global_simple(void);
uint8_t parse_import(void);
uint8_t parse_struct(void);
uint8_t parse_enum(void);
long long parse_statement(void);
long long parse_expr(void);
long long parse_primary(void);
long long parse_unary(void);
uint8_t parse_function(void);
uint8_t parse_program(void);
uint8_t parse_module(long long tok_end);
long long parse_body(void);
long long parse_if(void);
long long parse_while(long long hint_p);
long long parse_return(void);
uint8_t get_op_prec(void);
long long parse_binops(long long left, long long min_prec);
long long resolve_dot(long long base_node, long long name_p);
uint8_t add_token(long long tcode, long long start, long long len);
uint8_t is_alpha(long long ch);
uint8_t is_digit(long long ch);
uint8_t is_hex(long long ch);
uint8_t is_alnum(long long ch);
uint8_t is_space(long long ch);
uint8_t is_op(long long ch);
uint8_t is_punct(long long ch);
uint8_t skip_ws(void);
uint8_t check_kw(long long start, long long len);
uint8_t scan_comment(void);
uint8_t scan_word(void);
uint8_t scan_number(void);
uint8_t scan_string(void);
uint8_t scan_char(void);
uint8_t scan_op(void);
uint8_t scan_punct(void);
uint8_t scan_token(void);
uint8_t init_lexer(void);
uint8_t lex_all(void);
uint8_t lex_module(long long mi);
uint8_t init_types(void);
uint8_t ty_set_global_type(long long name_p, long long ty);
long long ty_get_global_type(long long name_p);
uint8_t ty_reset_scope(void);
long long ty_var_type(long long name_p);
uint8_t ty_set_var_type(long long name_p, long long ty);
uint8_t set_func_type(long long name_p, long long ty);
long long func_type(long long name_p);
long long promote(long long a, long long b);
long long type_of_lit(long long vstart, long long vlen);
long long add_type(long long nd);
uint8_t add_type_list(long long arr, long long cnt);
uint8_t save_fn_types(long long func_idx);
uint8_t save_fn_par_types(long long func_idx, long long params_arr, long long param_cnt);
long long get_fn_var_type(long long func_idx, long long name_p);
long long get_fn_ret_type(long long func_idx);
long long get_fn_par_type(long long func_idx, long long par_idx);
long long infer_one_func(long long func_idx);
long long propagate_call_params(void);
long long propagate_in_body(long long arr, long long cnt, long long caller_idx);
long long propagate_in_node(long long nd, long long caller_idx);
uint8_t is_int_only_op(long long op_p);
uint8_t uses_int_ops_list(long long arr, long long cnt, long long var_p);
uint8_t uses_int_ops_node(long long nd, long long var_p);
uint8_t find_func_idx(long long name_p);
uint8_t propagate_set_par_type(long long func_idx, long long par_idx, long long ty);
uint8_t infer_module(long long mi);
uint8_t run_types(void);
uint8_t init_dispatch(void);
uint8_t add_pure_extern(long long name_p);
uint8_t is_pure_extern(long long name_p);
uint8_t dsp_add_local(long long name_p);
uint8_t dsp_reset_locals(void);
uint8_t dsp_is_local(long long name_p);
uint8_t is_strided(long long name_p);
long long contains_var(long long nd, long long name_p);
long long find_loop_var(long long body_arr, long long body_cnt);
uint8_t find_strided(long long body_arr, long long body_cnt, long long loop_var_p);
uint8_t has_side_effects(long long nd);
long long classify_loop(long long wh);
uint8_t analyze_body(long long body_arr, long long body_cnt);
uint8_t run_dispatch(void);
uint8_t val_is_builtin(long long name_p);
uint8_t val_find_func(long long name_p);
uint8_t val_find_extern(long long name_p);
uint8_t val_check_node(long long nd);
uint8_t val_check_list(long long arr, long long cnt);
uint8_t run_validate(void);
uint8_t init_emitter(void);
uint8_t bridge_data(void);
uint8_t str_eq(long long p, long long lit, long long litlen);
uint8_t str_eq_packed(long long a, long long b);
uint8_t print_p(long long p);
uint8_t is_extern(long long name_p);
uint8_t mark_used(long long name_p);
uint8_t is_used(long long name_p);
uint8_t scan_calls(long long nd);
uint8_t find_used_externs(void);
uint8_t find_extern(long long name_p);
uint8_t is_ptr_type(long long tp);
uint8_t emit_val_cast(long long tp);
uint8_t emit_ctype(long long tp);
uint8_t emit_node(long long nd);
uint8_t emit_body(long long arr, long long cnt);
uint8_t emit_pad(void);
uint8_t emit_int(long long n);
uint8_t emit_fname(long long name_p);
uint8_t emit_includes(void);
uint8_t emit_q(long long s);
uint8_t emit_svc(void);
uint8_t emit_reg(long long reg, long long val);
uint8_t emit_runtime(void);
uint8_t emit_mem_model(void);
uint8_t emit_globals(void);
uint8_t emit_type_for(long long ty);
long long emit_get_par_hint(long long fi, long long j);
uint8_t emit_forward_decls(void);
uint8_t emit_functions(void);
uint8_t emit_main_entry(void);
uint8_t emit_all(void);
uint8_t init_enc_arm64(void);
uint8_t enc_emit32(long long val);
uint8_t enc_patch32(long long off, long long val);
long long enc_read32(long long off);
uint8_t enc_def_label(long long id);
uint8_t enc_def_label_at(long long id, long long off);
long long enc_new_label(void);
uint8_t enc_add_fixup(long long lbl, long long ty);
uint8_t enc_add_reloc(long long pos, long long sym, long long ty);
uint8_t enc_movz(long long rd, long long imm16, long long hw);
uint8_t enc_movk(long long rd, long long imm16, long long hw);
uint8_t enc_movn(long long rd, long long imm16, long long hw);
uint8_t enc_mov_wide(long long rd, long long val);
uint8_t enc_add_reg(long long rd, long long rn, long long rm);
uint8_t enc_sub_reg(long long rd, long long rn, long long rm);
uint8_t enc_and_reg(long long rd, long long rn, long long rm);
uint8_t enc_orr_reg(long long rd, long long rn, long long rm);
uint8_t enc_eor_reg(long long rd, long long rn, long long rm);
uint8_t enc_cmp_reg(long long rn, long long rm);
uint8_t enc_mvn(long long rd, long long rm);
uint8_t enc_neg(long long rd, long long rm);
uint8_t enc_mov_reg(long long rd, long long rm);
uint8_t enc_mul(long long rd, long long rn, long long rm);
uint8_t enc_sdiv(long long rd, long long rn, long long rm);
uint8_t enc_msub(long long rd, long long rn, long long rm, long long ra);
uint8_t enc_lslv(long long rd, long long rn, long long rm);
uint8_t enc_asrv(long long rd, long long rn, long long rm);
uint8_t enc_lsrv(long long rd, long long rn, long long rm);
uint8_t enc_add_imm(long long rd, long long rn, long long imm12);
uint8_t enc_sub_imm(long long rd, long long rn, long long imm12);
uint8_t enc_subs_imm(long long rd, long long rn, long long imm12);
uint8_t enc_cmp_imm(long long rn, long long imm12);
uint8_t enc_cset(long long rd, long long cond);
uint8_t enc_csneg(long long rd, long long rn, long long rm, long long cond);
uint8_t enc_ldr_uoff(long long rt, long long rn, long long uoff);
uint8_t enc_str_uoff(long long rt, long long rn, long long uoff);
uint8_t enc_ldrb_uoff(long long rt, long long rn, long long uoff);
uint8_t enc_strb_uoff(long long rt, long long rn, long long uoff);
uint8_t enc_ldrh_uoff(long long rt, long long rn, long long uoff);
uint8_t enc_strh_uoff(long long rt, long long rn, long long uoff);
uint8_t enc_ldr_w_uoff(long long rt, long long rn, long long uoff);
uint8_t enc_str_w_uoff(long long rt, long long rn, long long uoff);
uint8_t enc_ldrb_reg(long long rt, long long rn, long long rm);
uint8_t enc_ldrb_post(long long rt, long long rn, long long simm);
uint8_t enc_strb_post(long long rt, long long rn, long long simm);
uint8_t enc_str_pre(long long rt, long long rn, long long simm);
uint8_t enc_ldr_post(long long rt, long long rn, long long simm);
uint8_t enc_str_d_pre(long long rt, long long rn, long long simm);
uint8_t enc_ldr_d_post(long long rt, long long rn, long long simm);
uint8_t enc_stp_pre(long long rt1, long long rt2, long long rn, long long simm);
uint8_t enc_ldp_post(long long rt1, long long rt2, long long rn, long long simm);
uint8_t enc_b(long long off);
uint8_t enc_bl(long long off);
uint8_t enc_cbz(long long rt, long long off);
uint8_t enc_cbnz(long long rt, long long off);
uint8_t enc_b_cond(long long cond, long long off);
uint8_t enc_b_label(long long lbl);
uint8_t enc_bl_label(long long lbl);
uint8_t enc_cbz_label(long long rt, long long lbl);
uint8_t enc_cbnz_label(long long rt, long long lbl);
uint8_t enc_b_cond_label(long long cond, long long lbl);
uint8_t enc_blr(long long rn);
uint8_t enc_ret(void);
uint8_t enc_svc(long long imm16);
uint8_t enc_nop(void);
uint8_t enc_fadd(long long rd, long long rn, long long rm);
uint8_t enc_fsub(long long rd, long long rn, long long rm);
uint8_t enc_fmul(long long rd, long long rn, long long rm);
uint8_t enc_fdiv(long long rd, long long rn, long long rm);
uint8_t enc_fcmp(long long rn, long long rm);
uint8_t enc_fcmp_zero(long long rn);
uint8_t enc_fmov(long long rd, long long rn);
uint8_t enc_fneg(long long rd, long long rn);
uint8_t enc_scvtf(long long rd, long long rn);
uint8_t enc_fcvtzs(long long rd, long long rn);
uint8_t enc_fmov_to_gpr(long long rd, long long rn);
uint8_t enc_fmov_from_gpr(long long rd, long long rn);
uint8_t enc_ldr_d_uoff(long long rt, long long rn, long long uoff);
uint8_t enc_str_d_uoff(long long rt, long long rn, long long uoff);
uint8_t enc_ldr_reg_shift3(long long rt, long long rn, long long rm);
uint8_t enc_adrp(long long rd);
uint8_t enc_adr_label(long long rd, long long lbl);
uint8_t enc_resolve_fixups(void);
uint8_t init_codegen_arm64(void);
uint8_t bridge_data_arm64(void);
uint8_t a64_find_ext(long long name_p);
uint8_t a64_find_ext_by_argc(long long name_p, long long argc);
long long a64_ext_par_is_float(long long exi, long long par_idx);
long long a64_ext_ret_is_float(long long exi);
uint8_t a64_str_eq(long long p, long long lit, long long litlen);
uint8_t a64_str_eq_packed(long long a, long long b);
uint8_t a64_print_p(long long p);
uint8_t a64_print_dec(long long n);
long long a64_var_add(long long name_p);
uint8_t a64_var_idx(long long name_p);
long long a64_var_find(long long name_p);
uint8_t a64_pre_reg_vars(long long arr, long long cnt);
long long a64_parse_int(long long p);
uint8_t a64_is_float_lit(long long p);
long long a64_var_type(long long vi);
long long a64_var_is_float(long long vi);
uint8_t a64_emit_load_var(long long vi, long long off);
uint8_t a64_emit_store_var(long long vi, long long off, long long rty);
uint8_t a64_find_fn(long long name_p);
long long a64_new_lbl(void);
uint8_t a64_emit_mov(long long val);
uint8_t a64_emit_push(void);
uint8_t a64_emit_pop(long long r);
uint8_t a64_emit_fpush(void);
uint8_t a64_emit_fpop(long long r);
uint8_t a64_emit_node(long long nd);
uint8_t a64_emit_body(long long arr, long long cnt);
uint8_t a64_emit_stmt(long long nd);
uint8_t a64_emit_func(long long fi);
uint8_t a64_mod_init(void);
long long emit_module_arm64(long long mi);
uint8_t emit_all_arm64(void);
long long u32(long long x);
long long sha_rotr(long long x, long long n);
uint8_t sha_init(void);
uint8_t sha_transform(long long blk, long long h);
uint8_t sha256(long long data, long long len, long long out);
uint8_t init_macho(void);
uint8_t mo_packed_eq(long long a, long long b);
uint16_t mo_atof(long long s);
uint8_t mo_add_float(long long str_ptr);
uint8_t mo_add_string(long long src, long long len);
uint8_t mo_is_system_lib(long long lib_name);
uint8_t mo_add_got(long long fn_name, long long lib_name);
uint8_t mo_find_got(long long fn_name);
uint8_t mo_find_lib_ordinal(long long lib_name);
uint8_t mo_w8(long long v);
uint8_t mo_w32(long long v);
uint8_t mo_w64(long long lo, long long hi);
uint8_t mo_pad(long long n);
uint8_t mo_align(long long a);
uint8_t mo_wstr(long long s, long long slen, long long n);
uint8_t mo_wcstr(long long s, long long n);
long long mo_page_align(long long v);
uint8_t mo_write_header(void);
uint8_t mo_write_segment(long long name, long long nlen, long long vmaddr_lo, long long vmaddr_hi, long long vmsize, long long fileoff, long long filesize, long long maxprot, long long initprot, long long nsects);
uint8_t mo_write_section(long long sectname, long long snlen, long long segname, long long sgnlen, long long addr_lo, long long addr_hi, long long size, long long offset, long long al, long long flags);
uint8_t mo_write_lc_chained_fixups(void);
uint8_t mo_write_lc_exports_trie(void);
uint8_t mo_write_lc_symtab(void);
uint8_t mo_write_lc_dysymtab(void);
uint8_t mo_write_lc_dylinker(void);
uint8_t mo_write_lc_main(void);
uint8_t mo_write_lc_load_dylib(void);
uint8_t mo_write_chained_fixups(long long nseg);
uint8_t mo_write_chained_fixups_real(long long nseg, long long gl_count);
uint8_t mo_write_lc_load_dylib_named(long long lib_name);
long long mo_write_exports_trie(long long main_file_offset);
uint8_t mo_write_nlist(long long n_strx, long long n_type, long long n_sect, long long n_desc, long long n_value_lo, long long n_value_hi);
uint8_t mo_compute_layout(long long code_size, long long data_count);
uint8_t mo_resolve_relocations(long long gl_names, long long gl_count);
uint8_t write_macho(long long filename, long long main_label, long long gl_names, long long gl_count);
uint8_t mo_write_code_signature(void);
uint8_t mo_w32be(long long v);
uint8_t mo_w64be(long long v);
uint8_t init_x64_enc(void);
uint8_t x64_enc_emit8(long long val);
uint8_t x64_enc_emit32(long long val);
uint8_t x64_enc_emit64(long long val);
uint8_t x64_enc_patch32(long long off, long long val);
long long x64_enc_read32(long long off);
uint8_t x64_enc_def_label(long long id);
uint8_t x64_enc_def_label_at(long long id, long long off);
long long x64_enc_new_label(void);
uint8_t x64_enc_add_fixup(long long lbl);
uint8_t x64_enc_add_reloc(long long pos, long long sym, long long ty);
uint8_t x64_enc_rex(long long w, long long r, long long x, long long b);
uint8_t x64_enc_modrm(long long mod, long long reg, long long rm);
uint8_t x64_enc_mem(long long reg_field, long long base, long long disp);
uint8_t x64_enc_mov_reg(long long dst, long long src);
uint8_t x64_enc_mov_imm64(long long rd, long long imm);
uint8_t x64_enc_mov_imm32(long long rd, long long imm);
uint8_t x64_enc_xor_zero(long long rd);
uint8_t x64_enc_mov_wide(long long rd, long long val);
uint8_t x64_enc_add_reg(long long dst, long long src);
uint8_t x64_enc_sub_reg(long long dst, long long src);
uint8_t x64_enc_and_reg(long long dst, long long src);
uint8_t x64_enc_or_reg(long long dst, long long src);
uint8_t x64_enc_xor_reg(long long dst, long long src);
uint8_t x64_enc_cmp_reg(long long dst, long long src);
uint8_t x64_enc_test_reg(long long dst, long long src);
uint8_t x64_enc_add_imm(long long rd, long long imm32);
uint8_t x64_enc_sub_imm(long long rd, long long imm32);
uint8_t x64_enc_cmp_imm(long long rd, long long imm32);
uint8_t x64_enc_and_imm(long long rd, long long imm32);
uint8_t x64_enc_or_imm(long long rd, long long imm32);
uint8_t x64_enc_xor_imm(long long rd, long long imm32);
uint8_t x64_enc_neg(long long rd);
uint8_t x64_enc_not(long long rd);
uint8_t x64_enc_imul(long long dst, long long src);
uint8_t x64_enc_idiv(long long src);
uint8_t x64_enc_cqo(void);
uint8_t x64_enc_shl_cl(long long rd);
uint8_t x64_enc_shr_cl(long long rd);
uint8_t x64_enc_sar_cl(long long rd);
uint8_t x64_enc_shl_imm(long long rd, long long imm8);
uint8_t x64_enc_shr_imm(long long rd, long long imm8);
uint8_t x64_enc_sar_imm(long long rd, long long imm8);
uint8_t x64_enc_push(long long rd);
uint8_t x64_enc_pop(long long rd);
uint8_t x64_enc_load(long long dst, long long base, long long disp);
uint8_t x64_enc_store(long long src, long long base, long long disp);
uint8_t x64_enc_loadb(long long dst, long long base, long long disp);
uint8_t x64_enc_storeb(long long src, long long base, long long disp);
uint8_t x64_enc_loadh(long long dst, long long base, long long disp);
uint8_t x64_enc_storeh(long long src, long long base, long long disp);
uint8_t x64_enc_load_w(long long dst, long long base, long long disp);
uint8_t x64_enc_store_w(long long src, long long base, long long disp);
uint8_t x64_enc_lea(long long dst, long long base, long long disp);
uint8_t x64_enc_call_label(long long lbl);
uint8_t x64_enc_jmp_label(long long lbl);
uint8_t x64_enc_jcc_label(long long cc, long long lbl);
uint8_t x64_enc_call_reg(long long rd);
uint8_t x64_enc_jmp_reg(long long rd);
uint8_t x64_enc_ret(void);
uint8_t x64_enc_syscall(void);
uint8_t x64_enc_nop(void);
uint8_t x64_enc_setcc(long long cc, long long rd);
uint8_t x64_enc_movzx8(long long dst, long long src);
long long x64_enc_lea_rip(long long rd);
long long x64_enc_load_rip(long long rd);
uint8_t x64_enc_addsd(long long dst, long long src);
uint8_t x64_enc_subsd(long long dst, long long src);
uint8_t x64_enc_mulsd(long long dst, long long src);
uint8_t x64_enc_divsd(long long dst, long long src);
uint8_t x64_enc_ucomisd(long long dst, long long src);
uint8_t x64_enc_movsd_reg(long long dst, long long src);
uint8_t x64_enc_xorpd(long long dst, long long src);
uint8_t x64_enc_cvtsi2sd(long long dst, long long src);
uint8_t x64_enc_cvttsd2si(long long dst, long long src);
uint8_t x64_enc_movq_to_xmm(long long dst, long long src);
uint8_t x64_enc_movq_from_xmm(long long dst, long long src);
uint8_t x64_enc_load_sd(long long dst, long long base, long long disp);
uint8_t x64_enc_store_sd(long long src, long long base, long long disp);
long long x64_enc_load_sd_rip(long long dst);
uint8_t x64_enc_resolve_fixups(void);
uint8_t init_codegen_x86_64(void);
uint8_t bridge_data_x86_64(void);
uint8_t x64_str_eq(long long p, long long lit, long long litlen);
uint8_t x64_str_eq_packed(long long a, long long b);
long long x64_var_add(long long name_p);
uint8_t x64_var_idx(long long name_p);
long long x64_var_find(long long name_p);
uint8_t x64_pre_reg_vars(long long arr, long long cnt);
long long x64_parse_int(long long p);
uint8_t x64_is_float_lit(long long p);
long long x64_var_is_float(long long vi);
uint8_t x64_emit_load_var(long long vi, long long off);
uint8_t x64_emit_store_var(long long vi, long long off, long long rty);
uint8_t x64_find_fn(long long name_p);
uint8_t x64_find_ext(long long name_p);
uint8_t x64_find_ext_by_argc(long long name_p, long long argc);
uint8_t x64_arg_reg(long long i);
uint8_t x64_emit_push(void);
uint8_t x64_emit_pop(long long r);
uint8_t x64_emit_fpush(void);
uint8_t x64_emit_fpop(long long xr);
uint8_t x64_align_call(void);
uint8_t x64_unalign_call(long long padded);
uint8_t x64_emit_node(long long nd);
uint8_t x64_emit_body(long long arr, long long cnt);
uint8_t x64_emit_stmt(long long nd);
uint8_t x64_emit_func(long long fi);
uint8_t x64_mod_init(void);
long long emit_module_x86_64(long long mi);
long long x64_emit_start_stub(void);
long long emit_all_x86_64(void);
uint8_t init_elf(void);
uint8_t elf_w8(long long v);
uint8_t elf_w16(long long v);
uint8_t elf_w32(long long v);
uint8_t elf_w64(long long v);
uint8_t elf_pad(long long n);
uint8_t elf_align(long long a);
uint8_t elf_wcstr(long long s, long long n);
long long elf_page_align(long long v);
uint8_t elf_write_header(void);
uint8_t elf_write_phdr(long long p_type, long long p_flags, long long p_offset, long long p_vaddr, long long p_filesz, long long p_memsz, long long p_align);
uint8_t elf_compute_layout(long long code_size, long long gl_count);
uint8_t elf_resolve_relocations(long long gl_names, long long gl_count);
uint8_t elf_packed_eq(long long a, long long b);
uint8_t write_elf(long long filename, long long main_label, long long gl_names, long long gl_count);
uint8_t init_bo(void);
uint8_t bo_reset(void);
uint8_t bo_write_u8(long long val);
uint8_t bo_write_u16(long long val);
uint8_t bo_write_u32(long long val);
uint8_t bo_write_u64(long long val);
long long bo_read_u8(void);
long long bo_read_u16(void);
long long bo_read_u32(void);
long long bo_read_u64(void);
uint8_t bo_write_bytes(long long src, long long sz);
uint8_t bo_read_bytes(long long dst, long long sz);
uint8_t bo_write_str(long long pv);
long long bo_read_str(void);
uint8_t bo_save_file(long long fpath);
long long bo_load_file(long long fpath);
uint8_t bo_make_path(long long mi);
uint8_t bo_write_code_arm64(long long code_s, long long fn_s, long long str_s, long long flt_s, long long rel_s);
long long bo_load_code_arm64(void);
uint8_t bo_write_code_x64(long long code_s, long long fn_s, long long str_s, long long flt_s, long long rel_s);
long long bo_load_code_x64(void);
uint8_t bo_resolve_calls_arm64(void);
uint8_t bo_resolve_calls_x64(void);
uint8_t bo_write_header(long long mi, long long target);
uint8_t bo_check_header(long long mi);
uint8_t bo_write_export(long long mi);
uint8_t bo_read_export(long long mi);
uint8_t main_bpp(void);

long long pack(long long s, long long l) {
  return (s + (l * 0x100000000));
  return 0;
}

long long unpack_s(long long p) {
  return (p % 0x100000000);
  return 0;
}

long long unpack_l(long long p) {
  return (p / 0x100000000);
  return 0;
}

uint8_t init_defs(void) {
  NODE_SZ = 64;
  ND_TYPE = 0;
  ND_ITYPE = 56;
  FN_TYPE = 0;
  FN_NAME = 8;
  FN_PARS = 16;
  FN_PCNT = 24;
  FN_BODY = 32;
  FN_BCNT = 40;
  FN_HINTS = 48;
  EX_TYPE = 0;
  EX_NAME = 8;
  EX_LIB = 16;
  EX_RET = 24;
  EX_ARGS = 32;
  EX_ACNT = 40;
  T_LIT = 0;
  T_VAR = 1;
  T_BINOP = 2;
  T_UNARY = 3;
  T_ASSIGN = 4;
  T_MEMLD = 5;
  T_MEMST = 6;
  T_CALL = 7;
  T_IF = 8;
  T_WHILE = 9;
  T_RET = 10;
  T_DECL = 11;
  T_NOP = 12;
  T_BREAK = 13;
  TK_KW = 0;
  TK_IMP = 1;
  TK_ID = 2;
  TK_NUM = 3;
  TK_HEX = 4;
  TK_STR = 5;
  TK_OP = 6;
  TK_PUNCT = 7;
  TY_UNKNOWN = 0;
  TY_BYTE = 1;
  TY_QUART = 2;
  TY_HALF = 3;
  TY_WORD = 4;
  TY_PTR = 5;
  TY_FLOAT = 6;
  TY_STRUCT = 7;
  return 0;
  return 0;
}

uint8_t init_array(void) {
  _ARR_HDR = 16;
  _ARR_ELEM = 8;
  _ARR_INIT = 8;
  return 0;
  return 0;
}

long long arr_new(void) {
  long long raw;
  raw = (long long)malloc((_ARR_HDR + (_ARR_INIT * _ARR_ELEM)));
  *(long long*)((uintptr_t)((raw + 0))) = 0;
  *(long long*)((uintptr_t)((raw + 8))) = _ARR_INIT;
  return (raw + _ARR_HDR);
  return 0;
}

long long arr_grow(long long arr, long long new_cap) {
  long long len, raw_new, i;
  len = (*(long long*)((uintptr_t)((arr - 16))));
  raw_new = (long long)malloc((_ARR_HDR + (new_cap * _ARR_ELEM)));
  *(long long*)((uintptr_t)((raw_new + 0))) = len;
  *(long long*)((uintptr_t)((raw_new + 8))) = new_cap;
  i = 0;
  while ((i < len)) {
    *(long long*)((uintptr_t)(((raw_new + _ARR_HDR) + (i * _ARR_ELEM)))) = (*(long long*)((uintptr_t)((arr + (i * _ARR_ELEM)))));
    i = (i + 1);
  }
  (free((void*)(uintptr_t)((arr - _ARR_HDR))), 0);
  return (raw_new + _ARR_HDR);
  return 0;
}

long long arr_push(long long arr, long long val) {
  long long len, cap;
  if ((arr == 0)) {
    arr = arr_new();
  }
  len = (*(long long*)((uintptr_t)((arr - 16))));
  cap = (*(long long*)((uintptr_t)((arr - 8))));
  if ((len == cap)) {
    arr = arr_grow(arr, (cap * 2));
  }
  *(long long*)((uintptr_t)((arr + (len * _ARR_ELEM)))) = val;
  *(long long*)((uintptr_t)((arr - 16))) = (len + 1);
  return arr;
  return 0;
}

long long arr_pop(long long arr) {
  long long len, val;
  if (((*(long long*)((uintptr_t)((arr - 16)))) == 0)) {
    return 0;
  }
  len = ((*(long long*)((uintptr_t)((arr - 16)))) - 1);
  val = (*(long long*)((uintptr_t)((arr + (len * _ARR_ELEM)))));
  *(long long*)((uintptr_t)((arr - 16))) = len;
  return val;
  return 0;
}

long long arr_len(long long arr) {
  if ((arr == 0)) {
    return 0;
  }
  return (*(long long*)((uintptr_t)((arr - 16))));
  return 0;
}

long long arr_cap(long long arr) {
  if ((arr == 0)) {
    return 0;
  }
  return (*(long long*)((uintptr_t)((arr - 8))));
  return 0;
}

long long arr_get(long long arr, long long i) {
  return (*(long long*)((uintptr_t)((arr + (i * _ARR_ELEM)))));
  return 0;
}

uint8_t arr_set(long long arr, long long i, long long val) {
  *(long long*)((uintptr_t)((arr + (i * _ARR_ELEM)))) = val;
  return 0;
  return 0;
}

long long arr_last(long long arr) {
  return (*(long long*)((uintptr_t)((arr + (((*(long long*)((uintptr_t)((arr - 16)))) - 1) * _ARR_ELEM)))));
  return 0;
}

uint8_t arr_clear(long long arr) {
  if ((arr != 0)) {
    *(long long*)((uintptr_t)((arr - 16))) = 0;
  }
  return 0;
  return 0;
}

uint8_t arr_free(long long arr) {
  if ((arr != 0)) {
    (free((void*)(uintptr_t)((arr - _ARR_HDR))), 0);
  }
  return 0;
  return 0;
}

uint8_t init_internal(void) {
  tmp_arr = (long long)malloc(16384);
  list_stack = (long long)malloc(512);
  list_sp = 0;
  list_depth = 0;
  last_list_cnt = 0;
  return 0;
  return 0;
}

uint8_t out(long long s) {
  long long i, ch;
  i = 0;
  ch = (long long)((uint8_t*)((uintptr_t)(s)))[(i)];
  while ((ch != 0)) {
    (_bpp_scratch[0] = (uint8_t)(ch), bpp_sys_write(1, _bpp_scratch, 1));
    i = (i + 1);
    ch = (long long)((uint8_t*)((uintptr_t)(s)))[(i)];
  }
  return 0;
  return 0;
}

uint8_t buf_eq(long long a, long long b, long long len) {
  long long i;
  i = 0;
  while ((i < len)) {
    if ((((long long)(*(uint8_t*)((uintptr_t)((a + i))))) != (long long)((uint8_t*)((uintptr_t)(b)))[(i)])) {
      return 0;
    }
    i = (i + 1);
  }
  return 1;
  return 0;
}

uint8_t print_buf(long long start, long long len) {
  long long i;
  i = 0;
  while ((i < len)) {
    (_bpp_scratch[0] = (uint8_t)(((long long)(*(uint8_t*)((uintptr_t)((start + i)))))), bpp_sys_write(1, _bpp_scratch, 1));
    i = (i + 1);
  }
  return 0;
  return 0;
}

uint8_t packed_eq(long long p1, long long p2) {
  long long s1, l1, s2, l2, i;
  s1 = (p1 % 0x100000000);
  l1 = (p1 / 0x100000000);
  s2 = (p2 % 0x100000000);
  l2 = (p2 / 0x100000000);
  if ((l1 != l2)) {
    return 0;
  }
  i = 0;
  while ((i < l1)) {
    if ((((long long)(*(uint8_t*)((uintptr_t)(((vbuf + s1) + i))))) != ((long long)(*(uint8_t*)((uintptr_t)(((vbuf + s2) + i))))))) {
      return 0;
    }
    i = (i + 1);
  }
  return 1;
  return 0;
}

long long make_node(long long type) {
  long long nd;
  nd = (long long)malloc(64);
  *(long long*)((uintptr_t)((nd + 0))) = type;
  *(long long*)((uintptr_t)((nd + 8))) = 0;
  *(long long*)((uintptr_t)((nd + 16))) = 0;
  *(long long*)((uintptr_t)((nd + 24))) = 0;
  *(long long*)((uintptr_t)((nd + 32))) = 0;
  *(long long*)((uintptr_t)((nd + 40))) = 0;
  *(long long*)((uintptr_t)((nd + 48))) = 0;
  *(long long*)((uintptr_t)((nd + 56))) = 0;
  return nd;
  return 0;
}

uint8_t list_begin(void) {
  *(long long*)((uintptr_t)((list_stack + (list_depth * 8)))) = list_sp;
  list_depth = (list_depth + 1);
  return 0;
  return 0;
}

uint8_t list_push(long long val) {
  *(long long*)((uintptr_t)((tmp_arr + (list_sp * 8)))) = val;
  list_sp = (list_sp + 1);
  return 0;
  return 0;
}

long long list_end(void) {
  long long saved, cnt, arr, i;
  list_depth = (list_depth - 1);
  saved = (*(long long*)((uintptr_t)((list_stack + (list_depth * 8)))));
  cnt = (list_sp - saved);
  if ((cnt == 0)) {
    last_list_cnt = 0;
    list_sp = saved;
    return 0;
  }
  arr = (long long)malloc((cnt * 8));
  i = 0;
  while ((i < cnt)) {
    *(long long*)((uintptr_t)((arr + (i * 8)))) = (*(long long*)((uintptr_t)((tmp_arr + ((saved + i) * 8)))));
    i = (i + 1);
  }
  list_sp = saved;
  last_list_cnt = cnt;
  return arr;
  return 0;
}

uint8_t init_diag(void) {
  _diag_buf = (long long)malloc(4096);
  _diag_warn_cnt = 0;
  return 0;
  return 0;
}

uint8_t diag_ch(long long c) {
  (*(uint8_t*)((uintptr_t)(_diag_buf)) = (uint8_t)(c));
  bpp_sys_write(2, (void*)((uintptr_t)(_diag_buf)), 1);
  return 0;
  return 0;
}

uint8_t diag_str(long long s) {
  long long i, ch;
  i = 0;
  ch = (long long)((uint8_t*)((uintptr_t)(s)))[(i)];
  while ((ch != 0)) {
    diag_ch(ch);
    i = (i + 1);
    ch = (long long)((uint8_t*)((uintptr_t)(s)))[(i)];
  }
  return 0;
  return 0;
}

uint8_t diag_int(long long n) {
  if ((n < 0)) {
    diag_ch(45);
    n = (0 - n);
  }
  if ((n >= 10)) {
    diag_int((n / 10));
  }
  diag_ch((48 + (n % 10)));
  return 0;
  return 0;
}

uint8_t diag_packed(long long p) {
  long long s, l, i;
  s = unpack_s(p);
  l = unpack_l(p);
  i = 0;
  while ((i < l)) {
    diag_ch(((long long)(*(uint8_t*)((uintptr_t)(((vbuf + s) + i))))));
    i = (i + 1);
  }
  return 0;
  return 0;
}

uint8_t diag_buf(long long addr, long long len) {
  bpp_sys_write(2, (void*)((uintptr_t)(addr)), len);
  return 0;
  return 0;
}

uint8_t diag_newline(void) {
  diag_ch(10);
  return 0;
  return 0;
}

uint8_t diag_fatal(long long code) {
  diag_str((long long)"error[E");
  if ((code < 100)) {
    diag_ch(48);
  }
  if ((code < 10)) {
    diag_ch(48);
  }
  diag_int(code);
  diag_str((long long)"]: ");
  return 0;
  return 0;
}

uint8_t diag_warn(long long code) {
  diag_str((long long)"warning[W");
  if ((code < 100)) {
    diag_ch(48);
  }
  if ((code < 10)) {
    diag_ch(48);
  }
  diag_int(code);
  diag_str((long long)"]: ");
  _diag_warn_cnt = (_diag_warn_cnt + 1);
  return 0;
  return 0;
}

uint8_t diag_loc(long long tok_idx) {
  long long line, mi, fp, fs, fl, base_line;
  if ((tok_idx < 0)) {
    return 0;
  }
  line = (*(long long*)((uintptr_t)((tok_lines + (tok_idx * 8)))));
  mi = mod_idx((*(long long*)((uintptr_t)((tok_src_off + (tok_idx * 8))))));
  base_line = arr_get(diag_file_line_starts, mi);
  diag_str((long long)"  --> ");
  if ((arr_len(diag_file_starts) > 0)) {
    fp = arr_get(diag_file_names, mi);
    fs = unpack_s(fp);
    fl = unpack_l(fp);
    diag_buf((fname_buf + fs), fl);
  }
  diag_ch(58);
  diag_int((line - base_line));
  diag_newline();
  return 0;
  return 0;
}

uint8_t diag_show_line(long long tok_idx) {
  long long off, line_start, line_end, col, tlen, i, ch, lnum, mi, base_line;
  if ((tok_idx < 0)) {
    return 0;
  }
  off = (*(long long*)((uintptr_t)((tok_src_off + (tok_idx * 8)))));
  line_start = off;
  while ((line_start > 0)) {
    if ((((long long)(*(uint8_t*)((uintptr_t)(((src + line_start) - 1))))) == 10)) {
      line_start = (0 - 1);
    } else {
      line_start = (line_start - 1);
    }
  }
  if ((line_start < 0)) {
    line_start = ((off - ((0 - line_start) - 1)) + 1);
  }
  line_end = off;
  while ((line_end < src_len)) {
    if ((((long long)(*(uint8_t*)((uintptr_t)((src + line_end))))) == 10)) {
      line_end = (src_len + line_end);
    } else {
      line_end = (line_end + 1);
    }
  }
  if ((line_end >= src_len)) {
    line_end = (line_end - src_len);
  }
  col = (off - line_start);
  tlen = (*(long long*)((uintptr_t)(((toks + (tok_idx * 24)) + 16))));
  if ((tlen < 1)) {
    tlen = 1;
  }
  mi = mod_idx(off);
  base_line = arr_get(diag_file_line_starts, mi);
  lnum = ((*(long long*)((uintptr_t)((tok_lines + (tok_idx * 8))))) - base_line);
  diag_str((long long)"  ");
  diag_int(lnum);
  diag_str((long long)" | ");
  i = line_start;
  while ((i < line_end)) {
    ch = ((long long)(*(uint8_t*)((uintptr_t)((src + i)))));
    if ((ch == 9)) {
      diag_ch(32);
    } else {
      diag_ch(ch);
    }
    i = (i + 1);
  }
  diag_newline();
  diag_str((long long)"  ");
  i = lnum;
  if ((i == 0)) {
    diag_ch(32);
  }
  while ((i > 0)) {
    diag_ch(32);
    i = (i / 10);
  }
  diag_str((long long)" | ");
  i = 0;
  while ((i < col)) {
    diag_ch(32);
    i = (i + 1);
  }
  i = 0;
  while ((i < tlen)) {
    diag_ch(94);
    i = (i + 1);
  }
  diag_newline();
  return 0;
  return 0;
}

uint8_t diag_help(long long msg) {
  diag_str((long long)"  = help: ");
  diag_str(msg);
  diag_newline();
  return 0;
  return 0;
}

uint8_t diag_summary(void) {
  if ((_diag_warn_cnt > 0)) {
    diag_int(_diag_warn_cnt);
    diag_str((long long)" warning(s) emitted");
    diag_newline();
  }
  return 0;
  return 0;
}

uint8_t emit_ch(long long ch) {
  (*(uint8_t*)((uintptr_t)((outbuf + outbuf_len))) = (uint8_t)(ch));
  outbuf_len = (outbuf_len + 1);
  if ((ch == 10)) {
    outbuf_lines = (outbuf_lines + 1);
  }
  return 0;
  return 0;
}

uint8_t emit_str(long long s) {
  long long i, ch;
  i = 0;
  ch = (long long)((uint8_t*)((uintptr_t)(s)))[(i)];
  while ((ch != 0)) {
    (*(uint8_t*)((uintptr_t)((outbuf + outbuf_len))) = (uint8_t)(ch));
    outbuf_len = (outbuf_len + 1);
    i = (i + 1);
    ch = (long long)((uint8_t*)((uintptr_t)(s)))[(i)];
  }
  return 0;
  return 0;
}

uint8_t copy_cstr(long long dest, long long src) {
  long long i, ch;
  i = 0;
  ch = (long long)((uint8_t*)((uintptr_t)(src)))[(i)];
  while ((ch != 0)) {
    (*(uint8_t*)((uintptr_t)((dest + i))) = (uint8_t)(ch));
    i = (i + 1);
    ch = (long long)((uint8_t*)((uintptr_t)(src)))[(i)];
  }
  return i;
  return 0;
}

long long hash_mem(long long off, long long len) {
  long long h, i;
  h = 5381;
  i = 0;
  while ((i < len)) {
    h = ((h * 33) + ((long long)(*(uint8_t*)((uintptr_t)((off + i))))));
    i = (i + 1);
  }
  return h;
  return 0;
}

uint8_t last_slash(long long off, long long len) {
  long long i, r;
  r = (-1);
  i = 0;
  while ((i < len)) {
    if ((((long long)(*(uint8_t*)((uintptr_t)((off + i))))) == 47)) {
      r = i;
    }
    i = (i + 1);
  }
  return r;
  return 0;
}

uint8_t was_imported(long long path_off, long long path_len) {
  long long h, i;
  h = hash_mem(path_off, path_len);
  i = 0;
  while ((i < arr_len(imp_hashes))) {
    if ((arr_get(imp_hashes, i) == h)) {
      return 1;
    }
    i = (i + 1);
  }
  return 0;
  return 0;
}

uint8_t mark_imported(long long path_off, long long path_len) {
  imp_hashes = arr_push(imp_hashes, hash_mem(path_off, path_len));
  return 0;
  return 0;
}

long long mod_idx(long long src_off) {
  long long lo, hi, mid, ms;
  lo = 0;
  hi = (arr_len(diag_file_starts) - 1);
  while ((lo < hi)) {
    mid = (((lo + hi) + 1) / 2);
    ms = arr_get(diag_file_starts, mid);
    if ((ms <= src_off)) {
      lo = mid;
    } else {
      hi = (mid - 1);
    }
  }
  return lo;
  return 0;
}

uint8_t mod_idx_by_hash(long long path_off, long long path_len) {
  long long h, i;
  h = hash_mem(path_off, path_len);
  i = 0;
  while ((i < arr_len(imp_hashes))) {
    if ((arr_get(imp_hashes, i) == h)) {
      return i;
    }
    i = (i + 1);
  }
  return 0;
  return 0;
}

long long fnv1a(long long off, long long len) {
  long long h, i;
  h = 0xcbf29ce484222325;
  i = 0;
  while ((i < len)) {
    h = (h ^ ((long long)(*(uint8_t*)((uintptr_t)((off + i))))));
    h = (h * 0x100000001b3);
    i = (i + 1);
  }
  return h;
  return 0;
}

uint8_t topo_sort(void) {
  long long n, i, j, in_deg, queue, head, tail, u;
  n = arr_len(diag_file_starts);
  in_deg = (long long)malloc((n * 8));
  i = 0;
  while ((i < n)) {
    *(long long*)((uintptr_t)((in_deg + (i * 8)))) = 0;
    i = (i + 1);
  }
  i = 0;
  while ((i < arr_len(dep_from))) {
    j = arr_get(dep_from, i);
    *(long long*)((uintptr_t)((in_deg + (j * 8)))) = ((*(long long*)((uintptr_t)((in_deg + (j * 8))))) + 1);
    i = (i + 1);
  }
  queue = (long long)malloc((n * 8));
  head = 0;
  tail = 0;
  i = 0;
  while ((i < n)) {
    if (((*(long long*)((uintptr_t)((in_deg + (i * 8))))) == 0)) {
      *(long long*)((uintptr_t)((queue + (tail * 8)))) = i;
      tail = (tail + 1);
    }
    i = (i + 1);
  }
  mod_topo = arr_new();
  while ((head < tail)) {
    u = (*(long long*)((uintptr_t)((queue + (head * 8)))));
    head = (head + 1);
    mod_topo = arr_push(mod_topo, u);
    i = 0;
    while ((i < arr_len(dep_to))) {
      if ((arr_get(dep_to, i) == u)) {
        j = arr_get(dep_from, i);
        *(long long*)((uintptr_t)((in_deg + (j * 8)))) = ((*(long long*)((uintptr_t)((in_deg + (j * 8))))) - 1);
        if (((*(long long*)((uintptr_t)((in_deg + (j * 8))))) == 0)) {
          *(long long*)((uintptr_t)((queue + (tail * 8)))) = j;
          tail = (tail + 1);
        }
      }
      i = (i + 1);
    }
  }
  (free((void*)(uintptr_t)(in_deg)), 0);
  (free((void*)(uintptr_t)(queue)), 0);
  return 0;
  return 0;
}

long long hex_emit(long long buf, long long off, long long val) {
  long long i, nibble, ch;
  i = 15;
  while ((i >= 0)) {
    nibble = (val & 15);
    if ((nibble < 10)) {
      ch = (48 + nibble);
    } else {
      ch = (87 + nibble);
    }
    (*(uint8_t*)((uintptr_t)(((buf + off) + i))) = (uint8_t)(ch));
    val = ((val >> 4) & 0x0fffffffffffffff);
    i = (i - 1);
  }
  return (off + 16);
  return 0;
}

long long hex_parse(long long buf, long long off) {
  long long val, i, ch, d;
  val = 0;
  i = 0;
  while ((i < 16)) {
    ch = ((long long)(*(uint8_t*)((uintptr_t)(((buf + off) + i)))));
    d = 0;
    if ((ch >= 48)) {
      if ((ch <= 57)) {
        d = (ch - 48);
      }
    }
    if ((ch >= 97)) {
      if ((ch <= 102)) {
        d = (ch - 87);
      }
    }
    if ((ch >= 65)) {
      if ((ch <= 70)) {
        d = (ch - 55);
      }
    }
    val = ((val * 16) + d);
    i = (i + 1);
  }
  return val;
  return 0;
}

long long cache_manifest_hash(void) {
  long long h, n, i, fp, fs, fl, j;
  h = 0xcbf29ce484222325;
  n = arr_len(diag_file_starts);
  h = (h ^ (n & 0xFF));
  h = (h * 0x100000001b3);
  h = (h ^ ((n >> 8) & 0xFF));
  h = (h * 0x100000001b3);
  i = 0;
  while ((i < n)) {
    fp = arr_get(diag_file_names, i);
    fs = unpack_s(fp);
    fl = unpack_l(fp);
    j = 0;
    while ((j < fl)) {
      h = (h ^ ((long long)(*(uint8_t*)((uintptr_t)(((fname_buf + fs) + j))))));
      h = (h * 0x100000001b3);
      j = (j + 1);
    }
    h = (h ^ 0);
    h = (h * 0x100000001b3);
    i = (i + 1);
  }
  return h;
  return 0;
}

uint8_t cache_load(void) {
  long long fd, n, i, nread, bpos, line_start, hash_val, j, fname_len, manifest;
  n = arr_len(diag_file_starts);
  mod_stale = arr_new();
  i = 0;
  while ((i < n)) {
    mod_stale = arr_push(mod_stale, 1);
    i = (i + 1);
  }
  (*(uint8_t*)((uintptr_t)((pathbuf + 0))) = (uint8_t)(46));
  (*(uint8_t*)((uintptr_t)((pathbuf + 1))) = (uint8_t)(98));
  (*(uint8_t*)((uintptr_t)((pathbuf + 2))) = (uint8_t)(112));
  (*(uint8_t*)((uintptr_t)((pathbuf + 3))) = (uint8_t)(112));
  (*(uint8_t*)((uintptr_t)((pathbuf + 4))) = (uint8_t)(95));
  (*(uint8_t*)((uintptr_t)((pathbuf + 5))) = (uint8_t)(99));
  (*(uint8_t*)((uintptr_t)((pathbuf + 6))) = (uint8_t)(97));
  (*(uint8_t*)((uintptr_t)((pathbuf + 7))) = (uint8_t)(99));
  (*(uint8_t*)((uintptr_t)((pathbuf + 8))) = (uint8_t)(104));
  (*(uint8_t*)((uintptr_t)((pathbuf + 9))) = (uint8_t)(101));
  (*(uint8_t*)((uintptr_t)((pathbuf + 10))) = (uint8_t)(47));
  (*(uint8_t*)((uintptr_t)((pathbuf + 11))) = (uint8_t)(104));
  (*(uint8_t*)((uintptr_t)((pathbuf + 12))) = (uint8_t)(97));
  (*(uint8_t*)((uintptr_t)((pathbuf + 13))) = (uint8_t)(115));
  (*(uint8_t*)((uintptr_t)((pathbuf + 14))) = (uint8_t)(104));
  (*(uint8_t*)((uintptr_t)((pathbuf + 15))) = (uint8_t)(101));
  (*(uint8_t*)((uintptr_t)((pathbuf + 16))) = (uint8_t)(115));
  (*(uint8_t*)((uintptr_t)((pathbuf + 17))) = (uint8_t)(0));
  fd = bpp_sys_open((const char*)((uintptr_t)(pathbuf)), 0);
  if ((fd < 0)) {
    return 0;
  }
  nread = bpp_sys_read(fd, (void*)((uintptr_t)(_cache_buf)), 65536);
  bpp_sys_close(fd);
  if ((nread <= 0)) {
    return 0;
  }
  if ((nread < 17)) {
    return 0;
  }
  manifest = hex_parse(_cache_buf, 0);
  if ((manifest != cache_manifest_hash())) {
    return 0;
  }
  bpos = 17;
  while (((bpos + 17) < nread)) {
    hash_val = hex_parse(_cache_buf, bpos);
    bpos = (bpos + 17);
    line_start = bpos;
    while ((bpos < nread)) {
      if ((((long long)(*(uint8_t*)((uintptr_t)((_cache_buf + bpos))))) == 10)) {
        bpos = (bpos + nread);
      }
      if ((bpos < nread)) {
        bpos = (bpos + 1);
      }
    }
    if ((bpos >= nread)) {
      bpos = (bpos - nread);
    }
    fname_len = (bpos - line_start);
    bpos = (bpos + 1);
    i = 0;
    while ((i < n)) {
      long long fp, fs, fl, match;
      fp = arr_get(diag_file_names, i);
      fs = unpack_s(fp);
      fl = unpack_l(fp);
      if ((fl == fname_len)) {
        match = 1;
        j = 0;
        while ((j < fl)) {
          if ((((long long)(*(uint8_t*)((uintptr_t)(((fname_buf + fs) + j))))) != ((long long)(*(uint8_t*)((uintptr_t)(((_cache_buf + line_start) + j))))))) {
            match = 0;
          }
          j = (j + 1);
        }
        if (match) {
          if ((hash_val == arr_get(mod_hashes, i))) {
            arr_set(mod_stale, i, 0);
          }
        }
      }
      i = (i + 1);
    }
  }
  return 0;
  return 0;
}

uint8_t cache_save(void) {
  long long fd, n, i, bpos, fp, fs, fl, j;
  n = arr_len(diag_file_starts);
  (*(uint8_t*)((uintptr_t)((pathbuf + 0))) = (uint8_t)(46));
  (*(uint8_t*)((uintptr_t)((pathbuf + 1))) = (uint8_t)(98));
  (*(uint8_t*)((uintptr_t)((pathbuf + 2))) = (uint8_t)(112));
  (*(uint8_t*)((uintptr_t)((pathbuf + 3))) = (uint8_t)(112));
  (*(uint8_t*)((uintptr_t)((pathbuf + 4))) = (uint8_t)(95));
  (*(uint8_t*)((uintptr_t)((pathbuf + 5))) = (uint8_t)(99));
  (*(uint8_t*)((uintptr_t)((pathbuf + 6))) = (uint8_t)(97));
  (*(uint8_t*)((uintptr_t)((pathbuf + 7))) = (uint8_t)(99));
  (*(uint8_t*)((uintptr_t)((pathbuf + 8))) = (uint8_t)(104));
  (*(uint8_t*)((uintptr_t)((pathbuf + 9))) = (uint8_t)(101));
  (*(uint8_t*)((uintptr_t)((pathbuf + 10))) = (uint8_t)(47));
  (*(uint8_t*)((uintptr_t)((pathbuf + 11))) = (uint8_t)(104));
  (*(uint8_t*)((uintptr_t)((pathbuf + 12))) = (uint8_t)(97));
  (*(uint8_t*)((uintptr_t)((pathbuf + 13))) = (uint8_t)(115));
  (*(uint8_t*)((uintptr_t)((pathbuf + 14))) = (uint8_t)(104));
  (*(uint8_t*)((uintptr_t)((pathbuf + 15))) = (uint8_t)(101));
  (*(uint8_t*)((uintptr_t)((pathbuf + 16))) = (uint8_t)(115));
  (*(uint8_t*)((uintptr_t)((pathbuf + 17))) = (uint8_t)(0));
  bpos = 0;
  bpos = hex_emit(_cache_buf, bpos, cache_manifest_hash());
  (*(uint8_t*)((uintptr_t)((_cache_buf + bpos))) = (uint8_t)(10));
  bpos = (bpos + 1);
  i = 0;
  while ((i < n)) {
    bpos = hex_emit(_cache_buf, bpos, arr_get(mod_hashes, i));
    (*(uint8_t*)((uintptr_t)((_cache_buf + bpos))) = (uint8_t)(32));
    bpos = (bpos + 1);
    fp = arr_get(diag_file_names, i);
    fs = unpack_s(fp);
    fl = unpack_l(fp);
    j = 0;
    while ((j < fl)) {
      (*(uint8_t*)((uintptr_t)((_cache_buf + bpos))) = (uint8_t)(((long long)(*(uint8_t*)((uintptr_t)(((fname_buf + fs) + j)))))));
      bpos = (bpos + 1);
      j = (j + 1);
    }
    (*(uint8_t*)((uintptr_t)((_cache_buf + bpos))) = (uint8_t)(10));
    bpos = (bpos + 1);
    i = (i + 1);
  }
  fd = bpp_sys_open((const char*)((uintptr_t)(pathbuf)), 0x601);
  if ((fd < 0)) {
    return 0;
  }
  bpp_sys_write(fd, (void*)((uintptr_t)(_cache_buf)), bpos);
  bpp_sys_close(fd);
  return 0;
  return 0;
}

uint8_t propagate_stale(void) {
  long long i, j, mi;
  i = 0;
  while ((i < arr_len(mod_topo))) {
    mi = arr_get(mod_topo, i);
    if (arr_get(mod_stale, mi)) {
      j = 0;
      while ((j < arr_len(dep_to))) {
        if ((arr_get(dep_to, j) == mi)) {
          arr_set(mod_stale, arr_get(dep_from, j), 1);
        }
        j = (j + 1);
      }
    }
    i = (i + 1);
  }
  return 0;
  return 0;
}

uint8_t resolve_path(long long base_off, long long base_len, long long name_off, long long name_len) {
  long long sl, out, i;
  sl = last_slash(base_off, base_len);
  out = 0;
  if ((sl >= 0)) {
    i = 0;
    while ((i <= sl)) {
      (*(uint8_t*)((uintptr_t)((pathbuf + out))) = (uint8_t)(((long long)(*(uint8_t*)((uintptr_t)((base_off + i)))))));
      out = (out + 1);
      i = (i + 1);
    }
  }
  i = 0;
  while ((i < name_len)) {
    (*(uint8_t*)((uintptr_t)((pathbuf + out))) = (uint8_t)(((long long)(*(uint8_t*)((uintptr_t)((name_off + i)))))));
    out = (out + 1);
    i = (i + 1);
  }
  (*(uint8_t*)((uintptr_t)((pathbuf + out))) = (uint8_t)(0));
  return out;
  return 0;
}

uint8_t init_target(void) {
  _imp_target = (long long)malloc(16);
  (*(uint8_t*)((uintptr_t)((_imp_target + 0))) = (uint8_t)(109));
  (*(uint8_t*)((uintptr_t)((_imp_target + 1))) = (uint8_t)(97));
  (*(uint8_t*)((uintptr_t)((_imp_target + 2))) = (uint8_t)(99));
  (*(uint8_t*)((uintptr_t)((_imp_target + 3))) = (uint8_t)(111));
  (*(uint8_t*)((uintptr_t)((_imp_target + 4))) = (uint8_t)(115));
  _imp_target_len = 5;
  return 0;
  return 0;
}

long long find_file(long long base_off, long long base_len, long long name_off, long long name_len) {
  long long plen, fd, i;
  plen = resolve_path(base_off, base_len, name_off, name_len);
  fd = bpp_sys_open((const char*)((uintptr_t)(pathbuf)), 0);
  if ((fd >= 0)) {
    bpp_sys_close(fd);
    return plen;
  }
  (*(uint8_t*)((uintptr_t)((pathbuf + 0))) = (uint8_t)(115));
  (*(uint8_t*)((uintptr_t)((pathbuf + 1))) = (uint8_t)(116));
  (*(uint8_t*)((uintptr_t)((pathbuf + 2))) = (uint8_t)(98));
  (*(uint8_t*)((uintptr_t)((pathbuf + 3))) = (uint8_t)(47));
  i = 0;
  while ((i < name_len)) {
    (*(uint8_t*)((uintptr_t)(((pathbuf + 4) + i))) = (uint8_t)(((long long)(*(uint8_t*)((uintptr_t)((name_off + i)))))));
    i = (i + 1);
  }
  (*(uint8_t*)((uintptr_t)(((pathbuf + 4) + name_len))) = (uint8_t)(0));
  fd = bpp_sys_open((const char*)((uintptr_t)(pathbuf)), 0);
  if ((fd >= 0)) {
    bpp_sys_close(fd);
    return (4 + name_len);
  }
  (*(uint8_t*)((uintptr_t)((pathbuf + 0))) = (uint8_t)(100));
  (*(uint8_t*)((uintptr_t)((pathbuf + 1))) = (uint8_t)(114));
  (*(uint8_t*)((uintptr_t)((pathbuf + 2))) = (uint8_t)(105));
  (*(uint8_t*)((uintptr_t)((pathbuf + 3))) = (uint8_t)(118));
  (*(uint8_t*)((uintptr_t)((pathbuf + 4))) = (uint8_t)(101));
  (*(uint8_t*)((uintptr_t)((pathbuf + 5))) = (uint8_t)(114));
  (*(uint8_t*)((uintptr_t)((pathbuf + 6))) = (uint8_t)(115));
  (*(uint8_t*)((uintptr_t)((pathbuf + 7))) = (uint8_t)(47));
  i = 0;
  while ((i < name_len)) {
    (*(uint8_t*)((uintptr_t)(((pathbuf + 8) + i))) = (uint8_t)(((long long)(*(uint8_t*)((uintptr_t)((name_off + i)))))));
    i = (i + 1);
  }
  (*(uint8_t*)((uintptr_t)(((pathbuf + 8) + name_len))) = (uint8_t)(0));
  fd = bpp_sys_open((const char*)((uintptr_t)(pathbuf)), 0);
  if ((fd >= 0)) {
    bpp_sys_close(fd);
    return (8 + name_len);
  }
  (*(uint8_t*)((uintptr_t)((pathbuf + 0))) = (uint8_t)(115));
  (*(uint8_t*)((uintptr_t)((pathbuf + 1))) = (uint8_t)(114));
  (*(uint8_t*)((uintptr_t)((pathbuf + 2))) = (uint8_t)(99));
  (*(uint8_t*)((uintptr_t)((pathbuf + 3))) = (uint8_t)(47));
  i = 0;
  while ((i < name_len)) {
    (*(uint8_t*)((uintptr_t)(((pathbuf + 4) + i))) = (uint8_t)(((long long)(*(uint8_t*)((uintptr_t)((name_off + i)))))));
    i = (i + 1);
  }
  (*(uint8_t*)((uintptr_t)(((pathbuf + 4) + name_len))) = (uint8_t)(0));
  fd = bpp_sys_open((const char*)((uintptr_t)(pathbuf)), 0);
  if ((fd >= 0)) {
    bpp_sys_close(fd);
    return (4 + name_len);
  }
  long long prefix_len, pi;
  prefix_len = 20;
  (*(uint8_t*)((uintptr_t)((pathbuf + 0))) = (uint8_t)(47));
  (*(uint8_t*)((uintptr_t)((pathbuf + 1))) = (uint8_t)(117));
  (*(uint8_t*)((uintptr_t)((pathbuf + 2))) = (uint8_t)(115));
  (*(uint8_t*)((uintptr_t)((pathbuf + 3))) = (uint8_t)(114));
  (*(uint8_t*)((uintptr_t)((pathbuf + 4))) = (uint8_t)(47));
  (*(uint8_t*)((uintptr_t)((pathbuf + 5))) = (uint8_t)(108));
  (*(uint8_t*)((uintptr_t)((pathbuf + 6))) = (uint8_t)(111));
  (*(uint8_t*)((uintptr_t)((pathbuf + 7))) = (uint8_t)(99));
  (*(uint8_t*)((uintptr_t)((pathbuf + 8))) = (uint8_t)(97));
  (*(uint8_t*)((uintptr_t)((pathbuf + 9))) = (uint8_t)(108));
  (*(uint8_t*)((uintptr_t)((pathbuf + 10))) = (uint8_t)(47));
  (*(uint8_t*)((uintptr_t)((pathbuf + 11))) = (uint8_t)(108));
  (*(uint8_t*)((uintptr_t)((pathbuf + 12))) = (uint8_t)(105));
  (*(uint8_t*)((uintptr_t)((pathbuf + 13))) = (uint8_t)(98));
  (*(uint8_t*)((uintptr_t)((pathbuf + 14))) = (uint8_t)(47));
  (*(uint8_t*)((uintptr_t)((pathbuf + 15))) = (uint8_t)(98));
  (*(uint8_t*)((uintptr_t)((pathbuf + 16))) = (uint8_t)(112));
  (*(uint8_t*)((uintptr_t)((pathbuf + 17))) = (uint8_t)(112));
  (*(uint8_t*)((uintptr_t)((pathbuf + 18))) = (uint8_t)(47));
  i = 0;
  while ((i < name_len)) {
    (*(uint8_t*)((uintptr_t)(((pathbuf + 19) + i))) = (uint8_t)(((long long)(*(uint8_t*)((uintptr_t)((name_off + i)))))));
    i = (i + 1);
  }
  (*(uint8_t*)((uintptr_t)(((pathbuf + 19) + name_len))) = (uint8_t)(0));
  fd = bpp_sys_open((const char*)((uintptr_t)(pathbuf)), 0);
  if ((fd >= 0)) {
    bpp_sys_close(fd);
    return (19 + name_len);
  }
  prefix_len = 24;
  (*(uint8_t*)((uintptr_t)((pathbuf + 0))) = (uint8_t)(47));
  (*(uint8_t*)((uintptr_t)((pathbuf + 1))) = (uint8_t)(117));
  (*(uint8_t*)((uintptr_t)((pathbuf + 2))) = (uint8_t)(115));
  (*(uint8_t*)((uintptr_t)((pathbuf + 3))) = (uint8_t)(114));
  (*(uint8_t*)((uintptr_t)((pathbuf + 4))) = (uint8_t)(47));
  (*(uint8_t*)((uintptr_t)((pathbuf + 5))) = (uint8_t)(108));
  (*(uint8_t*)((uintptr_t)((pathbuf + 6))) = (uint8_t)(111));
  (*(uint8_t*)((uintptr_t)((pathbuf + 7))) = (uint8_t)(99));
  (*(uint8_t*)((uintptr_t)((pathbuf + 8))) = (uint8_t)(97));
  (*(uint8_t*)((uintptr_t)((pathbuf + 9))) = (uint8_t)(108));
  (*(uint8_t*)((uintptr_t)((pathbuf + 10))) = (uint8_t)(47));
  (*(uint8_t*)((uintptr_t)((pathbuf + 11))) = (uint8_t)(108));
  (*(uint8_t*)((uintptr_t)((pathbuf + 12))) = (uint8_t)(105));
  (*(uint8_t*)((uintptr_t)((pathbuf + 13))) = (uint8_t)(98));
  (*(uint8_t*)((uintptr_t)((pathbuf + 14))) = (uint8_t)(47));
  (*(uint8_t*)((uintptr_t)((pathbuf + 15))) = (uint8_t)(98));
  (*(uint8_t*)((uintptr_t)((pathbuf + 16))) = (uint8_t)(112));
  (*(uint8_t*)((uintptr_t)((pathbuf + 17))) = (uint8_t)(112));
  (*(uint8_t*)((uintptr_t)((pathbuf + 18))) = (uint8_t)(47));
  (*(uint8_t*)((uintptr_t)((pathbuf + 19))) = (uint8_t)(115));
  (*(uint8_t*)((uintptr_t)((pathbuf + 20))) = (uint8_t)(116));
  (*(uint8_t*)((uintptr_t)((pathbuf + 21))) = (uint8_t)(98));
  (*(uint8_t*)((uintptr_t)((pathbuf + 22))) = (uint8_t)(47));
  i = 0;
  while ((i < name_len)) {
    (*(uint8_t*)((uintptr_t)(((pathbuf + 23) + i))) = (uint8_t)(((long long)(*(uint8_t*)((uintptr_t)((name_off + i)))))));
    i = (i + 1);
  }
  (*(uint8_t*)((uintptr_t)(((pathbuf + 23) + name_len))) = (uint8_t)(0));
  fd = bpp_sys_open((const char*)((uintptr_t)(pathbuf)), 0);
  if ((fd >= 0)) {
    bpp_sys_close(fd);
    return (23 + name_len);
  }
  (*(uint8_t*)((uintptr_t)((pathbuf + 0))) = (uint8_t)(47));
  (*(uint8_t*)((uintptr_t)((pathbuf + 1))) = (uint8_t)(117));
  (*(uint8_t*)((uintptr_t)((pathbuf + 2))) = (uint8_t)(115));
  (*(uint8_t*)((uintptr_t)((pathbuf + 3))) = (uint8_t)(114));
  (*(uint8_t*)((uintptr_t)((pathbuf + 4))) = (uint8_t)(47));
  (*(uint8_t*)((uintptr_t)((pathbuf + 5))) = (uint8_t)(108));
  (*(uint8_t*)((uintptr_t)((pathbuf + 6))) = (uint8_t)(111));
  (*(uint8_t*)((uintptr_t)((pathbuf + 7))) = (uint8_t)(99));
  (*(uint8_t*)((uintptr_t)((pathbuf + 8))) = (uint8_t)(97));
  (*(uint8_t*)((uintptr_t)((pathbuf + 9))) = (uint8_t)(108));
  (*(uint8_t*)((uintptr_t)((pathbuf + 10))) = (uint8_t)(47));
  (*(uint8_t*)((uintptr_t)((pathbuf + 11))) = (uint8_t)(108));
  (*(uint8_t*)((uintptr_t)((pathbuf + 12))) = (uint8_t)(105));
  (*(uint8_t*)((uintptr_t)((pathbuf + 13))) = (uint8_t)(98));
  (*(uint8_t*)((uintptr_t)((pathbuf + 14))) = (uint8_t)(47));
  (*(uint8_t*)((uintptr_t)((pathbuf + 15))) = (uint8_t)(98));
  (*(uint8_t*)((uintptr_t)((pathbuf + 16))) = (uint8_t)(112));
  (*(uint8_t*)((uintptr_t)((pathbuf + 17))) = (uint8_t)(112));
  (*(uint8_t*)((uintptr_t)((pathbuf + 18))) = (uint8_t)(47));
  (*(uint8_t*)((uintptr_t)((pathbuf + 19))) = (uint8_t)(100));
  (*(uint8_t*)((uintptr_t)((pathbuf + 20))) = (uint8_t)(114));
  (*(uint8_t*)((uintptr_t)((pathbuf + 21))) = (uint8_t)(105));
  (*(uint8_t*)((uintptr_t)((pathbuf + 22))) = (uint8_t)(118));
  (*(uint8_t*)((uintptr_t)((pathbuf + 23))) = (uint8_t)(101));
  (*(uint8_t*)((uintptr_t)((pathbuf + 24))) = (uint8_t)(114));
  (*(uint8_t*)((uintptr_t)((pathbuf + 25))) = (uint8_t)(115));
  (*(uint8_t*)((uintptr_t)((pathbuf + 26))) = (uint8_t)(47));
  i = 0;
  while ((i < name_len)) {
    (*(uint8_t*)((uintptr_t)(((pathbuf + 27) + i))) = (uint8_t)(((long long)(*(uint8_t*)((uintptr_t)((name_off + i)))))));
    i = (i + 1);
  }
  (*(uint8_t*)((uintptr_t)(((pathbuf + 27) + name_len))) = (uint8_t)(0));
  fd = bpp_sys_open((const char*)((uintptr_t)(pathbuf)), 0);
  if ((fd >= 0)) {
    bpp_sys_close(fd);
    return (27 + name_len);
  }
  long long dot, base, new_len, j;
  dot = (0 - 1);
  i = (name_len - 1);
  while ((i >= 0)) {
    if ((((long long)(*(uint8_t*)((uintptr_t)((name_off + i))))) == 46)) {
      dot = i;
      i = 0;
    }
    i = (i - 1);
  }
  if ((dot > 0)) {
    long long nbuf;
    nbuf = (pathbuf + 512);
    i = 0;
    while ((i < dot)) {
      (*(uint8_t*)((uintptr_t)((nbuf + i))) = (uint8_t)(((long long)(*(uint8_t*)((uintptr_t)((name_off + i)))))));
      i = (i + 1);
    }
    (*(uint8_t*)((uintptr_t)((nbuf + dot))) = (uint8_t)(95));
    j = 0;
    while ((j < _imp_target_len)) {
      (*(uint8_t*)((uintptr_t)((((nbuf + dot) + 1) + j))) = (uint8_t)(((long long)(*(uint8_t*)((uintptr_t)((_imp_target + j)))))));
      j = (j + 1);
    }
    j = dot;
    while ((j < name_len)) {
      (*(uint8_t*)((uintptr_t)((((((nbuf + dot) + 1) + _imp_target_len) + j) - dot))) = (uint8_t)(((long long)(*(uint8_t*)((uintptr_t)((name_off + j)))))));
      j = (j + 1);
    }
    new_len = ((name_len + 1) + _imp_target_len);
    return find_file(base_off, base_len, nbuf, new_len);
  }
  return 0;
  return 0;
}

long long check_file_import(long long loff, long long llen) {
  long long p, q, ch, name_start, name_len;
  p = 0;
  while ((p < llen)) {
    ch = ((long long)(*(uint8_t*)((uintptr_t)((loff + p)))));
    if ((ch != 32)) {
      if ((ch != 9)) {
        ch = 0;
      }
    }
    if ((ch != 0)) {
      p = (p + 1);
    }
    if ((ch == 0)) {
      p = (llen + p);
    }
  }
  if ((p >= llen)) {
    p = (p - llen);
  }
  if (((p + 6) > llen)) {
    return 0;
  }
  if ((((long long)(*(uint8_t*)((uintptr_t)((loff + p))))) != 105)) {
    return 0;
  }
  if ((((long long)(*(uint8_t*)((uintptr_t)(((loff + p) + 1))))) != 109)) {
    return 0;
  }
  if ((((long long)(*(uint8_t*)((uintptr_t)(((loff + p) + 2))))) != 112)) {
    return 0;
  }
  if ((((long long)(*(uint8_t*)((uintptr_t)(((loff + p) + 3))))) != 111)) {
    return 0;
  }
  if ((((long long)(*(uint8_t*)((uintptr_t)(((loff + p) + 4))))) != 114)) {
    return 0;
  }
  if ((((long long)(*(uint8_t*)((uintptr_t)(((loff + p) + 5))))) != 116)) {
    return 0;
  }
  p = (p + 6);
  if ((p >= llen)) {
    return 0;
  }
  ch = ((long long)(*(uint8_t*)((uintptr_t)((loff + p)))));
  if ((ch != 32)) {
    if ((ch != 9)) {
      return 0;
    }
  }
  while ((p < llen)) {
    ch = ((long long)(*(uint8_t*)((uintptr_t)((loff + p)))));
    if ((ch != 32)) {
      if ((ch != 9)) {
        ch = 0;
      }
    }
    if ((ch != 0)) {
      p = (p + 1);
    }
    if ((ch == 0)) {
      p = (llen + p);
    }
  }
  if ((p >= llen)) {
    p = (p - llen);
  }
  if ((p >= llen)) {
    return 0;
  }
  if ((((long long)(*(uint8_t*)((uintptr_t)((loff + p))))) != 34)) {
    return 0;
  }
  p = (p + 1);
  name_start = p;
  while ((p < llen)) {
    if ((((long long)(*(uint8_t*)((uintptr_t)((loff + p))))) == 34)) {
      name_len = (p - name_start);
      p = (p + 1);
      while ((p < llen)) {
        ch = ((long long)(*(uint8_t*)((uintptr_t)((loff + p)))));
        if ((ch != 32)) {
          if ((ch != 9)) {
            ch = 0;
          }
        }
        if ((ch != 0)) {
          p = (p + 1);
        }
        if ((ch == 0)) {
          p = (llen + p);
        }
      }
      if ((p >= llen)) {
        p = (p - llen);
      }
      if ((p >= llen)) {
        return 0;
      }
      if ((((long long)(*(uint8_t*)((uintptr_t)((loff + p))))) != 59)) {
        return 0;
      }
      q = 0;
      while ((q < name_len)) {
        (*(uint8_t*)((uintptr_t)((linebuf + q))) = (uint8_t)(((long long)(*(uint8_t*)((uintptr_t)(((loff + name_start) + q)))))));
        q = (q + 1);
      }
      return name_len;
    }
    p = (p + 1);
  }
  return 0;
  return 0;
}

uint8_t process_file(long long path_off, long long path_len) {
  long long fd, nread, fsize, pos, line_start, ch;
  long long fname_len, plen, i;
  long long saved_top, my_path, my_file, my_mod_idx;
  if (was_imported(path_off, path_len)) {
    return 0;
  }
  mark_imported(path_off, path_len);
  saved_top = fbuf_top;
  my_path = fbuf_top;
  i = 0;
  while ((i < path_len)) {
    (*(uint8_t*)((uintptr_t)(((fbuf + my_path) + i))) = (uint8_t)(((long long)(*(uint8_t*)((uintptr_t)((path_off + i)))))));
    i = (i + 1);
  }
  fbuf_top = (fbuf_top + path_len);
  my_file = fbuf_top;
  i = 0;
  while ((i < path_len)) {
    (*(uint8_t*)((uintptr_t)((pathbuf + i))) = (uint8_t)(((long long)(*(uint8_t*)((uintptr_t)(((fbuf + my_path) + i)))))));
    i = (i + 1);
  }
  (*(uint8_t*)((uintptr_t)((pathbuf + path_len))) = (uint8_t)(0));
  fd = bpp_sys_open((const char*)((uintptr_t)(pathbuf)), 0);
  if ((fd < 0)) {
    diag_fatal(1);
    diag_str((long long)"cannot open file '");
    long long _ei;
    _ei = 0;
    while ((_ei < path_len)) {
      diag_ch(((long long)(*(uint8_t*)((uintptr_t)((pathbuf + _ei))))));
      _ei = (_ei + 1);
    }
    diag_str((long long)"'");
    diag_newline();
    bpp_sys_exit(1);
  }
  fsize = 0;
  nread = bpp_sys_read(fd, (void*)((uintptr_t)(((fbuf + my_file) + fsize))), 65536);
  while ((nread > 0)) {
    fsize = (fsize + nread);
    nread = bpp_sys_read(fd, (void*)((uintptr_t)(((fbuf + my_file) + fsize))), 65536);
  }
  bpp_sys_close(fd);
  fbuf_top = (my_file + fsize);
  long long fn_start;
  fn_start = fname_pos;
  i = 0;
  while ((i < path_len)) {
    (*(uint8_t*)((uintptr_t)((fname_buf + fname_pos))) = (uint8_t)(((long long)(*(uint8_t*)((uintptr_t)(((fbuf + my_path) + i)))))));
    fname_pos = (fname_pos + 1);
    i = (i + 1);
  }
  diag_file_starts = arr_push(diag_file_starts, outbuf_len);
  diag_file_names = arr_push(diag_file_names, pack(fn_start, path_len));
  diag_file_line_starts = arr_push(diag_file_line_starts, outbuf_lines);
  mod_hashes = arr_push(mod_hashes, fnv1a((fbuf + my_file), fsize));
  my_mod_idx = (arr_len(diag_file_starts) - 1);
  pos = 0;
  while ((pos < fsize)) {
    line_start = pos;
    while ((pos < fsize)) {
      if ((((long long)(*(uint8_t*)((uintptr_t)(((fbuf + my_file) + pos))))) == 10)) {
        pos = (pos + 1);
        pos = (fsize + pos);
      }
      if ((pos <= fsize)) {
        pos = (pos + 1);
      }
    }
    if ((pos > fsize)) {
      pos = (pos - fsize);
    }
    fname_len = check_file_import(((fbuf + my_file) + line_start), (pos - line_start));
    if ((fname_len > 0)) {
      plen = find_file((fbuf + my_path), path_len, linebuf, fname_len);
      if ((plen > 0)) {
        process_file(pathbuf, plen);
        dep_from = arr_push(dep_from, my_mod_idx);
        dep_to = arr_push(dep_to, mod_idx_by_hash(pathbuf, plen));
      } else {
        diag_fatal(2);
        diag_str((long long)"import '");
        long long _ei;
        _ei = 0;
        while ((_ei < fname_len)) {
          diag_ch(((long long)(*(uint8_t*)((uintptr_t)((linebuf + _ei))))));
          _ei = (_ei + 1);
        }
        diag_str((long long)"' not found (searched: ./, stb/, /usr/local/lib/bpp/stb/)");
        diag_newline();
        bpp_sys_exit(1);
      }
    } else {
      i = line_start;
      while ((i < pos)) {
        emit_ch(((long long)(*(uint8_t*)((uintptr_t)(((fbuf + my_file) + i))))));
        i = (i + 1);
      }
    }
  }
  fbuf_top = saved_top;
  return 0;
  return 0;
}

uint8_t init_import(void) {
  fbuf = (long long)malloc(262144);
  fbuf_top = 0;
  imp_hashes = arr_new();
  pathbuf = (long long)malloc(4096);
  linebuf = (long long)malloc(4096);
  outbuf = (long long)malloc(1048576);
  outbuf_len = 0;
  fname_buf = (long long)malloc(65536);
  fname_pos = 0;
  diag_file_starts = arr_new();
  diag_file_names = arr_new();
  diag_file_line_starts = arr_new();
  outbuf_lines = 0;
  mod_hashes = arr_new();
  dep_from = arr_new();
  dep_to = arr_new();
  _cache_buf = (long long)malloc(65536);
  init_target();
  return 0;
  return 0;
}

uint8_t init_parser(void) {
  toks = (long long)malloc(4194304);
  vbuf = (long long)malloc(1048576);
  vbuf_pos = 0;
  tok_cnt = 0;
  tok_pos = 0;
  globals = arr_new();
  glob_cnt = 0;
  externs = arr_new();
  ext_cnt = 0;
  funcs = arr_new();
  func_cnt = 0;
  func_mods = arr_new();
  glob_mods = arr_new();
  struct_mods = arr_new();
  enum_mods = arr_new();
  extern_mods = arr_new();
  sd_names = arr_new();
  sd_fields = arr_new();
  sd_fcounts = arr_new();
  sd_sizes = arr_new();
  vt_names = arr_new();
  vt_sidx = arr_new();
  var_stack_names = arr_new();
  ev_names = arr_new();
  ev_values = arr_new();
  TK_FLOAT = 8;
  _prim_hint = 0;
  return 0;
  return 0;
}

long long add_struct_def(long long name_p) {
  sd_names = arr_push(sd_names, name_p);
  sd_fields = arr_push(sd_fields, (long long)malloc((64 * 8)));
  sd_fcounts = arr_push(sd_fcounts, 0);
  sd_sizes = arr_push(sd_sizes, 0);
  struct_mods = arr_push(struct_mods, tok_mod(tok_pos));
  return (arr_len(sd_names) - 1);
  return 0;
}

uint8_t add_struct_field(long long idx, long long field_p) {
  long long fields, fc;
  fields = arr_get(sd_fields, idx);
  fc = arr_get(sd_fcounts, idx);
  *(long long*)((uintptr_t)((fields + (fc * 8)))) = field_p;
  fc = (fc + 1);
  arr_set(sd_fcounts, idx, fc);
  arr_set(sd_sizes, idx, (fc * 8));
  return 0;
  return 0;
}

uint8_t find_struct(long long name_p) {
  long long i;
  i = 0;
  while ((i < arr_len(sd_names))) {
    if (packed_eq(arr_get(sd_names, i), name_p)) {
      return i;
    }
    i = (i + 1);
  }
  return (-1);
  return 0;
}

uint8_t get_field_offset(long long idx, long long field_p) {
  long long fields, fc, i;
  fields = arr_get(sd_fields, idx);
  fc = arr_get(sd_fcounts, idx);
  i = 0;
  while ((i < fc)) {
    if (packed_eq((*(long long*)((uintptr_t)((fields + (i * 8))))), field_p)) {
      return (i * 8);
    }
    i = (i + 1);
  }
  return (-1);
  return 0;
}

long long get_struct_size(long long idx) {
  return arr_get(sd_sizes, idx);
  return 0;
}

uint8_t set_var_type(long long var_p, long long idx) {
  vt_names = arr_push(vt_names, var_p);
  vt_sidx = arr_push(vt_sidx, idx);
  return 0;
  return 0;
}

long long get_var_type(long long var_p) {
  long long i;
  i = (arr_len(vt_names) - 1);
  while ((i >= 0)) {
    if (packed_eq(arr_get(vt_names, i), var_p)) {
      return arr_get(vt_sidx, i);
    }
    i = (i - 1);
  }
  return (-1);
  return 0;
}

uint8_t set_var_stack(long long var_p) {
  var_stack_names = arr_push(var_stack_names, var_p);
  return 0;
  return 0;
}

uint8_t is_var_stack(long long var_p) {
  long long i;
  i = (arr_len(var_stack_names) - 1);
  while ((i >= 0)) {
    if (packed_eq(arr_get(var_stack_names, i), var_p)) {
      return 1;
    }
    i = (i - 1);
  }
  return 0;
  return 0;
}

uint8_t add_enum_val(long long name_p, long long val) {
  ev_names = arr_push(ev_names, name_p);
  ev_values = arr_push(ev_values, val);
  enum_mods = arr_push(enum_mods, tok_mod(tok_pos));
  return 0;
  return 0;
}

long long find_enum_val(long long name_p) {
  long long i;
  i = (arr_len(ev_names) - 1);
  while ((i >= 0)) {
    if (packed_eq(arr_get(ev_names, i), name_p)) {
      return arr_get(ev_values, i);
    }
    i = (i - 1);
  }
  return (0 - 1);
  return 0;
}

long long parse_lit_int(void) {
  long long vs, vl, i, ch, val, t;
  t = peek_type();
  vs = peek_vstart();
  vl = peek_vlen();
  val = 0;
  if ((t == TK_HEX)) {
    i = 2;
    while ((i < vl)) {
      ch = ((long long)(*(uint8_t*)((uintptr_t)(((vbuf + vs) + i)))));
      val = (val * 16);
      if ((ch >= 48)) {
        if ((ch <= 57)) {
          val = ((val + ch) - 48);
        }
      }
      if ((ch >= 65)) {
        if ((ch <= 70)) {
          val = ((val + ch) - 55);
        }
      }
      if ((ch >= 97)) {
        if ((ch <= 102)) {
          val = ((val + ch) - 87);
        }
      }
      i = (i + 1);
    }
  } else {
    i = 0;
    while ((i < vl)) {
      ch = ((long long)(*(uint8_t*)((uintptr_t)(((vbuf + vs) + i)))));
      val = (((val * 10) + ch) - 48);
      i = (i + 1);
    }
  }
  return val;
  return 0;
}

long long make_int_lit(long long val) {
  long long start, node, tmp, digit, i, j, t;
  start = vbuf_pos;
  if ((val == 0)) {
    (*(uint8_t*)((uintptr_t)((vbuf + vbuf_pos))) = (uint8_t)(48));
    vbuf_pos = (vbuf_pos + 1);
  } else {
    tmp = val;
    while ((tmp > 0)) {
      digit = (tmp % 10);
      (*(uint8_t*)((uintptr_t)((vbuf + vbuf_pos))) = (uint8_t)((48 + digit)));
      vbuf_pos = (vbuf_pos + 1);
      tmp = (tmp / 10);
    }
    i = start;
    j = (vbuf_pos - 1);
    while ((i < j)) {
      t = ((long long)(*(uint8_t*)((uintptr_t)((vbuf + i)))));
      (*(uint8_t*)((uintptr_t)((vbuf + i))) = (uint8_t)(((long long)(*(uint8_t*)((uintptr_t)((vbuf + j)))))));
      (*(uint8_t*)((uintptr_t)((vbuf + j))) = (uint8_t)(t));
      i = (i + 1);
      j = (j - 1);
    }
  }
  node = make_node(T_LIT);
  *(long long*)((uintptr_t)((node + 8))) = pack(start, (vbuf_pos - start));
  return node;
  return 0;
}

long long make_plus_op(void) {
  long long start;
  start = vbuf_pos;
  (*(uint8_t*)((uintptr_t)((vbuf + vbuf_pos))) = (uint8_t)(43));
  vbuf_pos = (vbuf_pos + 1);
  return pack(start, 1);
  return 0;
}

long long peek_type(void) {
  if ((tok_pos >= tok_cnt)) {
    return (-1);
  }
  return (*(long long*)((uintptr_t)((toks + (tok_pos * 24)))));
  return 0;
}

long long peek_type_at(long long off) {
  if (((tok_pos + off) >= tok_cnt)) {
    return (-1);
  }
  return (*(long long*)((uintptr_t)((toks + ((tok_pos + off) * 24)))));
  return 0;
}

long long peek_vstart(void) {
  return (*(long long*)((uintptr_t)(((toks + (tok_pos * 24)) + 8))));
  return 0;
}

long long peek_vlen(void) {
  return (*(long long*)((uintptr_t)(((toks + (tok_pos * 24)) + 16))));
  return 0;
}

long long cur_packed(void) {
  return pack(peek_vstart(), peek_vlen());
  return 0;
}

long long cur_type(void) {
  return peek_type();
  return 0;
}

uint8_t consume(void) {
  tok_pos = (tok_pos + 1);
  return 0;
  return 0;
}

long long val_is_ch(long long c) {
  long long addr;
  if ((tok_pos >= tok_cnt)) {
    return 0;
  }
  addr = (toks + (tok_pos * 24));
  if (((*(long long*)((uintptr_t)((addr + 16)))) != 1)) {
    return 0;
  }
  return (((long long)(*(uint8_t*)((uintptr_t)((vbuf + (*(long long*)((uintptr_t)((addr + 8))))))))) == c);
  return 0;
}

uint8_t val_is(long long s, long long slen) {
  long long addr, vs, vl;
  if ((tok_pos >= tok_cnt)) {
    return 0;
  }
  addr = (toks + (tok_pos * 24));
  vs = (*(long long*)((uintptr_t)((addr + 8))));
  vl = (*(long long*)((uintptr_t)((addr + 16))));
  if ((vl != slen)) {
    return 0;
  }
  return buf_eq((vbuf + vs), s, slen);
  return 0;
}

long long val_at_ch(long long off, long long c) {
  long long addr;
  if (((tok_pos + off) >= tok_cnt)) {
    return 0;
  }
  addr = (toks + ((tok_pos + off) * 24));
  if (((*(long long*)((uintptr_t)((addr + 16)))) != 1)) {
    return 0;
  }
  return (((long long)(*(uint8_t*)((uintptr_t)((vbuf + (*(long long*)((uintptr_t)((addr + 8))))))))) == c);
  return 0;
}

long long tok_mod(long long tidx) {
  return mod_idx((*(long long*)((uintptr_t)((tok_src_off + (tidx * 8))))));
  return 0;
}

uint8_t add_global(long long p) {
  globals = arr_push(globals, p);
  glob_cnt = arr_len(globals);
  glob_mods = arr_push(glob_mods, tok_mod(tok_pos));
  return 0;
  return 0;
}

uint8_t try_type_annotation(long long var_p) {
  long long type_p, idx, tl, ts;
  _prim_hint = 0;
  if (val_is_ch(58)) {
    consume();
    type_p = cur_packed();
    tl = unpack_l(type_p);
    ts = (vbuf + unpack_s(type_p));
    if ((tl == 5)) {
      if (buf_eq(ts, (long long)"float", 5)) {
        _prim_hint = TY_FLOAT;
        consume();
        return 0;
      }
    }
    if ((tl == 3)) {
      if (buf_eq(ts, (long long)"int", 3)) {
        _prim_hint = TY_WORD;
        consume();
        return 0;
      }
    }
    if ((tl == 4)) {
      if (buf_eq(ts, (long long)"half", 4)) {
        _prim_hint = TY_HALF;
        consume();
        return 0;
      }
      if (buf_eq(ts, (long long)"byte", 4)) {
        _prim_hint = TY_BYTE;
        consume();
        return 0;
      }
      if (buf_eq(ts, (long long)"word", 4)) {
        _prim_hint = TY_WORD;
        consume();
        return 0;
      }
    }
    if ((tl == 7)) {
      if (buf_eq(ts, (long long)"quarter", 7)) {
        _prim_hint = TY_QUART;
        consume();
        return 0;
      }
    }
    idx = find_struct(type_p);
    if ((idx >= 0)) {
      set_var_type(var_p, idx);
    } else {
      diag_fatal(101);
      diag_str((long long)"unknown type '");
      diag_packed(type_p);
      diag_str((long long)"' -- define it with 'struct'");
      diag_newline();
      diag_loc(tok_pos);
      diag_show_line(tok_pos);
      diag_help((long long)"declare the type with 'struct <name> { ... };' before using it");
      bpp_sys_exit(1);
    }
    consume();
  }
  return 0;
  return 0;
}

uint8_t parse_global(void) {
  long long var_p;
  consume();
  var_p = cur_packed();
  add_global(var_p);
  consume();
  try_type_annotation(var_p);
  while ((val_is_ch(59) == 0)) {
    if (val_is_ch(44)) {
      consume();
    }
    var_p = cur_packed();
    add_global(var_p);
    consume();
    try_type_annotation(var_p);
  }
  consume();
  return 0;
  return 0;
}

uint8_t parse_global_simple(void) {
  add_global(cur_packed());
  consume();
  if (val_is_ch(59)) {
    consume();
  }
  return 0;
  return 0;
}

uint8_t parse_import(void) {
  long long lib_p, ret_p, name_p, args_arr, ext;
  long long tp, ts, tl, pi;
  consume();
  if ((peek_type() == TK_STR)) {
    if (val_at_ch(1, 59)) {
      consume();
      consume();
      return 0;
    }
  }
  lib_p = pack((peek_vstart() + 1), (peek_vlen() - 2));
  consume();
  consume();
  while ((val_is_ch(125) == 0)) {
    tp = cur_packed();
    consume();
    if (val_is_ch(42)) {
      ts = vbuf_pos;
      tl = unpack_l(tp);
      pi = 0;
      while ((pi < tl)) {
        (*(uint8_t*)((uintptr_t)((vbuf + vbuf_pos))) = (uint8_t)(((long long)(*(uint8_t*)((uintptr_t)(((vbuf + unpack_s(tp)) + pi)))))));
        vbuf_pos = (vbuf_pos + 1);
        pi = (pi + 1);
      }
      (*(uint8_t*)((uintptr_t)((vbuf + vbuf_pos))) = (uint8_t)(42));
      vbuf_pos = (vbuf_pos + 1);
      tp = pack(ts, (tl + 1));
      consume();
    }
    ret_p = tp;
    name_p = cur_packed();
    consume();
    consume();
    list_begin();
    while ((val_is_ch(41) == 0)) {
      tp = cur_packed();
      consume();
      if (val_is_ch(42)) {
        ts = vbuf_pos;
        tl = unpack_l(tp);
        pi = 0;
        while ((pi < tl)) {
          (*(uint8_t*)((uintptr_t)((vbuf + vbuf_pos))) = (uint8_t)(((long long)(*(uint8_t*)((uintptr_t)(((vbuf + unpack_s(tp)) + pi)))))));
          vbuf_pos = (vbuf_pos + 1);
          pi = (pi + 1);
        }
        (*(uint8_t*)((uintptr_t)((vbuf + vbuf_pos))) = (uint8_t)(42));
        vbuf_pos = (vbuf_pos + 1);
        tp = pack(ts, (tl + 1));
        consume();
      }
      list_push(tp);
      if (val_is_ch(44)) {
        consume();
      }
    }
    consume();
    consume();
    args_arr = list_end();
    ext = (long long)malloc(NODE_SZ);
    *(long long*)((uintptr_t)((ext + EX_TYPE))) = 21;
    *(long long*)((uintptr_t)((ext + EX_NAME))) = name_p;
    *(long long*)((uintptr_t)((ext + EX_LIB))) = lib_p;
    *(long long*)((uintptr_t)((ext + EX_RET))) = ret_p;
    *(long long*)((uintptr_t)((ext + EX_ARGS))) = args_arr;
    *(long long*)((uintptr_t)((ext + EX_ACNT))) = last_list_cnt;
    externs = arr_push(externs, ext);
    ext_cnt = arr_len(externs);
    extern_mods = arr_push(extern_mods, tok_mod(tok_pos));
  }
  consume();
  return 0;
  return 0;
}

uint8_t parse_struct(void) {
  long long name_p, idx;
  consume();
  name_p = cur_packed();
  consume();
  consume();
  idx = add_struct_def(name_p);
  while ((val_is_ch(125) == 0)) {
    add_struct_field(idx, cur_packed());
    consume();
    if (val_is_ch(58)) {
      consume();
      consume();
    }
    if (val_is_ch(44)) {
      consume();
    }
  }
  consume();
  return 0;
  return 0;
}

uint8_t parse_enum(void) {
  long long name_p, val, member_p;
  consume();
  name_p = cur_packed();
  consume();
  consume();
  val = 0;
  while ((val_is_ch(125) == 0)) {
    member_p = cur_packed();
    consume();
    if (val_is_ch(61)) {
      consume();
      val = parse_lit_int();
      consume();
    }
    add_enum_val(member_p, val);
    val = (val + 1);
    if (val_is_ch(44)) {
      consume();
    }
  }
  consume();
  return 0;
  return 0;
}

long long parse_statement(void) {
  long long node, var_p, hint_p;
  hint_p = 0;
  if ((peek_type() == TK_PUNCT)) {
    if (val_is_ch(64)) {
      consume();
      hint_p = cur_packed();
      consume();
    }
  }
  if ((peek_type() == TK_KW)) {
    if (val_is((long long)"auto", 4)) {
      long long decl_hint, dh_arr, dh_cnt, dh_has;
      consume();
      decl_hint = 0;
      dh_arr = (long long)malloc(64);
      dh_cnt = 0;
      dh_has = 0;
      list_begin();
      var_p = cur_packed();
      list_push(var_p);
      consume();
      try_type_annotation(var_p);
      *(long long*)((uintptr_t)((dh_arr + (dh_cnt * 8)))) = _prim_hint;
      if ((_prim_hint > 0)) {
        decl_hint = _prim_hint;
        dh_has = 1;
      }
      dh_cnt = (dh_cnt + 1);
      while ((val_is_ch(59) == 0)) {
        if (val_is_ch(44)) {
          consume();
        }
        var_p = cur_packed();
        list_push(var_p);
        consume();
        try_type_annotation(var_p);
        *(long long*)((uintptr_t)((dh_arr + (dh_cnt * 8)))) = _prim_hint;
        if ((_prim_hint > 0)) {
          decl_hint = _prim_hint;
          dh_has = 1;
        }
        dh_cnt = (dh_cnt + 1);
      }
      consume();
      node = make_node(T_DECL);
      *(long long*)((uintptr_t)((node + 8))) = list_end();
      *(long long*)((uintptr_t)((node + 16))) = last_list_cnt;
      *(long long*)((uintptr_t)((node + 24))) = decl_hint;
      if (dh_has) {
        *(long long*)((uintptr_t)((node + 40))) = dh_arr;
      } else {
        *(long long*)((uintptr_t)((node + 40))) = 0;
        (free((void*)(uintptr_t)(dh_arr)), 0);
      }
      return node;
    }
    if (((val_is((long long)"byte", 4) || val_is((long long)"half", 4)) || val_is((long long)"quarter", 7))) {
      long long kw_hint;
      if (val_is((long long)"byte", 4)) {
        kw_hint = TY_BYTE;
      }
      if (val_is((long long)"half", 4)) {
        kw_hint = TY_HALF;
      }
      if (val_is((long long)"quarter", 7)) {
        kw_hint = TY_QUART;
      }
      consume();
      list_begin();
      var_p = cur_packed();
      list_push(var_p);
      consume();
      while ((val_is_ch(59) == 0)) {
        if (val_is_ch(44)) {
          consume();
        }
        var_p = cur_packed();
        list_push(var_p);
        consume();
      }
      consume();
      node = make_node(T_DECL);
      *(long long*)((uintptr_t)((node + 8))) = list_end();
      *(long long*)((uintptr_t)((node + 16))) = last_list_cnt;
      *(long long*)((uintptr_t)((node + 24))) = kw_hint;
      return node;
    }
    if (val_is((long long)"var", 3)) {
      consume();
      var_p = cur_packed();
      consume();
      try_type_annotation(var_p);
      set_var_stack(var_p);
      consume();
      node = make_node(T_DECL);
      list_begin();
      list_push(var_p);
      *(long long*)((uintptr_t)((node + 8))) = list_end();
      *(long long*)((uintptr_t)((node + 16))) = 1;
      *(long long*)((uintptr_t)((node + 32))) = 1;
      return node;
    }
    if (val_is((long long)"if", 2)) {
      return parse_if();
    }
    if (val_is((long long)"while", 5)) {
      return parse_while(hint_p);
    }
    if (val_is((long long)"return", 6)) {
      return parse_return();
    }
    if (val_is((long long)"break", 5)) {
      consume();
      if (val_is_ch(59)) {
        consume();
      }
      return make_node(T_BREAK);
    }
  }
  node = parse_expr();
  if (val_is_ch(59)) {
    consume();
  }
  return node;
  return 0;
}

long long parse_expr(void) {
  long long left, deref, node;
  if ((peek_type() == TK_OP)) {
    if (val_is_ch(42)) {
      consume();
      deref = parse_primary();
      if (val_is_ch(61)) {
        consume();
        node = make_node(T_MEMST);
        *(long long*)((uintptr_t)((node + 8))) = deref;
        *(long long*)((uintptr_t)((node + 16))) = parse_expr();
        return node;
      }
      left = make_node(T_MEMLD);
      *(long long*)((uintptr_t)((left + 8))) = deref;
      return parse_binops(left, 0);
    }
    if (val_is_ch(45)) {
      left = parse_unary();
      return parse_binops(left, 0);
    }
    if (val_is_ch(126)) {
      left = parse_unary();
      return parse_binops(left, 0);
    }
  }
  left = parse_primary();
  if (val_is_ch(61)) {
    consume();
    if (((*(long long*)((uintptr_t)((left + 0)))) == T_MEMLD)) {
      node = make_node(T_MEMST);
      *(long long*)((uintptr_t)((node + 8))) = (*(long long*)((uintptr_t)((left + 8))));
      *(long long*)((uintptr_t)((node + 16))) = parse_expr();
      return node;
    }
    node = make_node(T_ASSIGN);
    *(long long*)((uintptr_t)((node + 8))) = left;
    *(long long*)((uintptr_t)((node + 16))) = parse_expr();
    return node;
  }
  return parse_binops(left, 0);
  return 0;
}

long long parse_primary(void) {
  long long t, node, name_p, args_arr, type_p, sidx;
  t = peek_type();
  if ((t == TK_KW)) {
    if (val_is((long long)"sizeof", 6)) {
      consume();
      consume();
      type_p = cur_packed();
      consume();
      consume();
      sidx = find_struct(type_p);
      if ((sidx >= 0)) {
        return make_int_lit(get_struct_size(sidx));
      }
      diag_fatal(103);
      diag_str((long long)"sizeof applied to unknown type '");
      diag_packed(type_p);
      diag_str((long long)"'");
      diag_newline();
      diag_loc(tok_pos);
      diag_show_line(tok_pos);
      diag_help((long long)"sizeof requires a struct type defined with 'struct <name> { ... };'");
      bpp_sys_exit(1);
    }
  }
  if (((((t == TK_NUM) | (t == TK_HEX)) | (t == TK_STR)) | (t == TK_FLOAT))) {
    node = make_node(T_LIT);
    *(long long*)((uintptr_t)((node + 8))) = cur_packed();
    consume();
    return node;
  }
  if ((t == TK_ID)) {
    name_p = cur_packed();
    consume();
    if (val_is_ch(40)) {
      consume();
      list_begin();
      while ((val_is_ch(41) == 0)) {
        list_push(parse_expr());
        if (val_is_ch(44)) {
          consume();
        }
      }
      consume();
      args_arr = list_end();
      node = make_node(T_CALL);
      *(long long*)((uintptr_t)((node + 8))) = name_p;
      *(long long*)((uintptr_t)((node + 16))) = args_arr;
      *(long long*)((uintptr_t)((node + 24))) = last_list_cnt;
      return node;
    }
    long long ev;
    ev = find_enum_val(name_p);
    if ((ev >= 0)) {
      return make_int_lit(ev);
    }
    node = make_node(T_VAR);
    *(long long*)((uintptr_t)((node + 8))) = name_p;
    if (val_is_ch(46)) {
      node = resolve_dot(node, name_p);
    }
    return node;
  }
  if (val_is_ch(40)) {
    consume();
    node = parse_expr();
    consume();
    return node;
  }
  diag_fatal(104);
  if ((cur_type() == TK_KW)) {
    diag_str((long long)"keyword '");
    diag_packed(cur_packed());
    diag_str((long long)"' cannot be used as an expression");
  } else {
    diag_str((long long)"unexpected token '");
    diag_packed(cur_packed());
    diag_str((long long)"' in expression");
  }
  diag_newline();
  diag_loc(tok_pos);
  diag_show_line(tok_pos);
  if ((cur_type() == TK_KW)) {
    diag_help((long long)"keywords like 'auto', 'if', 'while' are statements, not values");
  }
  consume();
  bpp_sys_exit(1);
  return make_node(T_NOP);
  return 0;
}

long long parse_unary(void) {
  long long node, op_p;
  if ((peek_type() == TK_OP)) {
    if (val_is_ch(42)) {
      consume();
      node = make_node(T_MEMLD);
      *(long long*)((uintptr_t)((node + 8))) = parse_primary();
      return node;
    }
    if (val_is_ch(45)) {
      op_p = cur_packed();
      consume();
      node = make_node(T_UNARY);
      *(long long*)((uintptr_t)((node + 8))) = op_p;
      *(long long*)((uintptr_t)((node + 16))) = parse_unary();
      return node;
    }
    if (val_is_ch(126)) {
      op_p = cur_packed();
      consume();
      node = make_node(T_UNARY);
      *(long long*)((uintptr_t)((node + 8))) = op_p;
      *(long long*)((uintptr_t)((node + 16))) = parse_unary();
      return node;
    }
  }
  return parse_primary();
  return 0;
}

uint8_t parse_function(void) {
  long long name_p, params_arr, param_cnt, body_arr, body_cnt, func, var_p;
  long long ph_arr, ph_cnt, has_hints, my_mod;
  my_mod = tok_mod(tok_pos);
  name_p = cur_packed();
  consume();
  consume();
  ph_arr = (long long)malloc(64);
  ph_cnt = 0;
  has_hints = 0;
  list_begin();
  while ((val_is_ch(41) == 0)) {
    var_p = cur_packed();
    list_push(var_p);
    consume();
    try_type_annotation(var_p);
    *(long long*)((uintptr_t)((ph_arr + (ph_cnt * 8)))) = _prim_hint;
    if ((_prim_hint > 0)) {
      has_hints = 1;
    }
    ph_cnt = (ph_cnt + 1);
    if (val_is_ch(44)) {
      consume();
    }
  }
  consume();
  params_arr = list_end();
  param_cnt = last_list_cnt;
  consume();
  list_begin();
  while ((val_is_ch(125) == 0)) {
    list_push(parse_statement());
  }
  consume();
  body_arr = list_end();
  body_cnt = last_list_cnt;
  func = (long long)malloc(NODE_SZ);
  *(long long*)((uintptr_t)((func + FN_TYPE))) = 22;
  *(long long*)((uintptr_t)((func + FN_NAME))) = name_p;
  *(long long*)((uintptr_t)((func + FN_PARS))) = params_arr;
  *(long long*)((uintptr_t)((func + FN_PCNT))) = param_cnt;
  *(long long*)((uintptr_t)((func + FN_BODY))) = body_arr;
  *(long long*)((uintptr_t)((func + FN_BCNT))) = body_cnt;
  if (has_hints) {
    *(long long*)((uintptr_t)((func + FN_HINTS))) = ph_arr;
  } else {
    *(long long*)((uintptr_t)((func + FN_HINTS))) = 0;
    (free((void*)(uintptr_t)(ph_arr)), 0);
  }
  funcs = arr_push(funcs, func);
  func_cnt = arr_len(funcs);
  func_mods = arr_push(func_mods, my_mod);
  return 0;
  return 0;
}

uint8_t parse_program(void) {
  while ((tok_pos < tok_cnt)) {
    if ((peek_type() == TK_IMP)) {
      parse_import();
    } else {
      if ((peek_type() == TK_KW)) {
        if (val_is((long long)"auto", 4)) {
          parse_global();
        } else {
          if (val_is((long long)"struct", 6)) {
            parse_struct();
          } else {
            if (val_is((long long)"enum", 4)) {
              parse_enum();
            } else {
              consume();
            }
          }
        }
      } else {
        if ((peek_type() == TK_ID)) {
          if (val_at_ch(1, 40)) {
            parse_function();
          } else {
            parse_global_simple();
          }
        } else {
          consume();
        }
      }
    }
  }
  return 0;
  return 0;
}

uint8_t parse_module(long long tok_end) {
  while ((tok_pos < tok_end)) {
    if ((peek_type() == TK_IMP)) {
      parse_import();
    } else {
      if ((peek_type() == TK_KW)) {
        if (val_is((long long)"auto", 4)) {
          parse_global();
        } else {
          if (val_is((long long)"struct", 6)) {
            parse_struct();
          } else {
            if (val_is((long long)"enum", 4)) {
              parse_enum();
            } else {
              consume();
            }
          }
        }
      } else {
        if ((peek_type() == TK_ID)) {
          if (val_at_ch(1, 40)) {
            parse_function();
          } else {
            parse_global_simple();
          }
        } else {
          consume();
        }
      }
    }
  }
  return 0;
  return 0;
}

long long parse_body(void) {
  list_begin();
  if (val_is_ch(123)) {
    consume();
    while ((val_is_ch(125) == 0)) {
      list_push(parse_statement());
    }
    consume();
  } else {
    list_push(parse_statement());
  }
  return list_end();
  return 0;
}

long long parse_if(void) {
  long long cond, body_arr, body_cnt, else_arr, else_cnt, node;
  consume();
  consume();
  cond = parse_expr();
  consume();
  body_arr = parse_body();
  body_cnt = last_list_cnt;
  else_arr = 0;
  else_cnt = 0;
  if ((peek_type() == TK_KW)) {
    if (val_is((long long)"else", 4)) {
      consume();
      else_arr = parse_body();
      else_cnt = last_list_cnt;
    }
  }
  node = make_node(T_IF);
  *(long long*)((uintptr_t)((node + 8))) = cond;
  *(long long*)((uintptr_t)((node + 16))) = body_arr;
  *(long long*)((uintptr_t)((node + 24))) = body_cnt;
  *(long long*)((uintptr_t)((node + 32))) = else_arr;
  *(long long*)((uintptr_t)((node + 40))) = else_cnt;
  return node;
  return 0;
}

long long parse_while(long long hint_p) {
  long long cond, body_arr, node;
  consume();
  consume();
  cond = parse_expr();
  consume();
  body_arr = parse_body();
  node = make_node(T_WHILE);
  *(long long*)((uintptr_t)((node + 8))) = cond;
  *(long long*)((uintptr_t)((node + 16))) = body_arr;
  *(long long*)((uintptr_t)((node + 24))) = last_list_cnt;
  *(long long*)((uintptr_t)((node + 32))) = hint_p;
  return node;
  return 0;
}

long long parse_return(void) {
  long long node;
  consume();
  node = make_node(T_RET);
  *(long long*)((uintptr_t)((node + 8))) = parse_expr();
  if (val_is_ch(59)) {
    consume();
  }
  return node;
  return 0;
}

uint8_t get_op_prec(void) {
  long long vs, vl, c0, c1;
  if ((peek_type() != TK_OP)) {
    return (-1);
  }
  vs = peek_vstart();
  vl = peek_vlen();
  if ((vl == 1)) {
    c0 = ((long long)(*(uint8_t*)((uintptr_t)((vbuf + vs)))));
    if ((c0 == 124)) {
      return 3;
    }
    if ((c0 == 94)) {
      return 4;
    }
    if ((c0 == 38)) {
      return 5;
    }
    if ((c0 == 60)) {
      return 7;
    }
    if ((c0 == 62)) {
      return 7;
    }
    if ((c0 == 43)) {
      return 9;
    }
    if ((c0 == 45)) {
      return 9;
    }
    if ((c0 == 42)) {
      return 10;
    }
    if ((c0 == 47)) {
      return 10;
    }
    if ((c0 == 37)) {
      return 10;
    }
    return (-1);
  }
  if ((vl == 2)) {
    c0 = ((long long)(*(uint8_t*)((uintptr_t)((vbuf + vs)))));
    c1 = ((long long)(*(uint8_t*)((uintptr_t)(((vbuf + vs) + 1)))));
    if ((c0 == 124)) {
      if ((c1 == 124)) {
        return 1;
      }
    }
    if ((c0 == 38)) {
      if ((c1 == 38)) {
        return 2;
      }
    }
    if ((c0 == 61)) {
      if ((c1 == 61)) {
        return 6;
      }
    }
    if ((c0 == 33)) {
      if ((c1 == 61)) {
        return 6;
      }
    }
    if ((c0 == 60)) {
      if ((c1 == 61)) {
        return 7;
      }
    }
    if ((c0 == 62)) {
      if ((c1 == 61)) {
        return 7;
      }
    }
    if ((c0 == 60)) {
      if ((c1 == 60)) {
        return 8;
      }
    }
    if ((c0 == 62)) {
      if ((c1 == 62)) {
        return 8;
      }
    }
  }
  return (-1);
  return 0;
}

long long parse_binops(long long left, long long min_prec) {
  long long prec, op_p, right, node;
  while ((get_op_prec() >= min_prec)) {
    prec = get_op_prec();
    op_p = cur_packed();
    consume();
    right = parse_unary();
    while ((get_op_prec() > prec)) {
      right = parse_binops(right, get_op_prec());
    }
    node = make_node(T_BINOP);
    *(long long*)((uintptr_t)((node + 8))) = op_p;
    *(long long*)((uintptr_t)((node + 16))) = left;
    *(long long*)((uintptr_t)((node + 24))) = right;
    left = node;
  }
  return left;
  return 0;
}

long long resolve_dot(long long base_node, long long name_p) {
  long long vtype, field_p, offset, ptr_node, plus_op, off_node;
  vtype = get_var_type(name_p);
  while (val_is_ch(46)) {
    if ((vtype < 0)) {
      return base_node;
    }
    consume();
    field_p = cur_packed();
    consume();
    offset = get_field_offset(vtype, field_p);
    if ((offset < 0)) {
      diag_fatal(102);
      diag_str((long long)"struct has no field '");
      diag_packed(field_p);
      diag_str((long long)"'");
      diag_newline();
      diag_loc(tok_pos);
      diag_show_line(tok_pos);
      diag_help((long long)"check the struct definition for available field names");
      bpp_sys_exit(1);
    }
    plus_op = make_plus_op();
    off_node = make_int_lit(offset);
    ptr_node = make_node(T_BINOP);
    *(long long*)((uintptr_t)((ptr_node + 8))) = plus_op;
    *(long long*)((uintptr_t)((ptr_node + 16))) = base_node;
    *(long long*)((uintptr_t)((ptr_node + 24))) = off_node;
    base_node = make_node(T_MEMLD);
    *(long long*)((uintptr_t)((base_node + 8))) = ptr_node;
  }
  return base_node;
  return 0;
}

uint8_t add_token(long long tcode, long long start, long long len) {
  long long i, base;
  i = 0;
  while ((i < len)) {
    (*(uint8_t*)((uintptr_t)(((vbuf + vbuf_pos) + i))) = (uint8_t)(((long long)(*(uint8_t*)((uintptr_t)(((src + start) + i)))))));
    i = (i + 1);
  }
  base = (toks + (tok_cnt * 24));
  *(long long*)((uintptr_t)(base)) = tcode;
  *(long long*)((uintptr_t)((base + 8))) = vbuf_pos;
  *(long long*)((uintptr_t)((base + 16))) = len;
  *(long long*)((uintptr_t)((tok_lines + (tok_cnt * 8)))) = cur_line;
  *(long long*)((uintptr_t)((tok_src_off + (tok_cnt * 8)))) = start;
  vbuf_pos = (vbuf_pos + len);
  tok_cnt = (tok_cnt + 1);
  return 0;
  return 0;
}

uint8_t is_alpha(long long ch) {
  if ((ch >= 65)) {
    if ((ch <= 90)) {
      return 1;
    }
  }
  if ((ch >= 97)) {
    if ((ch <= 122)) {
      return 1;
    }
  }
  return 0;
  return 0;
}

uint8_t is_digit(long long ch) {
  if ((ch >= 48)) {
    if ((ch <= 57)) {
      return 1;
    }
  }
  return 0;
  return 0;
}

uint8_t is_hex(long long ch) {
  if (is_digit(ch)) {
    return 1;
  }
  if ((ch >= 65)) {
    if ((ch <= 70)) {
      return 1;
    }
  }
  if ((ch >= 97)) {
    if ((ch <= 102)) {
      return 1;
    }
  }
  return 0;
  return 0;
}

uint8_t is_alnum(long long ch) {
  if (is_alpha(ch)) {
    return 1;
  }
  if (is_digit(ch)) {
    return 1;
  }
  if ((ch == 95)) {
    return 1;
  }
  return 0;
  return 0;
}

uint8_t is_space(long long ch) {
  if (((((ch == 32) | (ch == 9)) | (ch == 10)) | (ch == 13))) {
    return 1;
  }
  return 0;
  return 0;
}

uint8_t is_op(long long ch) {
  if (((((ch == 43) | (ch == 45)) | (ch == 42)) | (ch == 47))) {
    return 1;
  }
  if ((((ch == 37) | (ch == 61)) | (ch == 33))) {
    return 1;
  }
  if (((((ch == 60) | (ch == 62)) | (ch == 124)) | (ch == 38))) {
    return 1;
  }
  if (((ch == 94) | (ch == 126))) {
    return 1;
  }
  return 0;
  return 0;
}

uint8_t is_punct(long long ch) {
  if (((((ch == 40) | (ch == 41)) | (ch == 123)) | (ch == 125))) {
    return 1;
  }
  if (((((ch == 91) | (ch == 93)) | (ch == 59)) | (ch == 44))) {
    return 1;
  }
  if ((((ch == 46) | (ch == 58)) | (ch == 64))) {
    return 1;
  }
  return 0;
  return 0;
}

uint8_t skip_ws(void) {
  while ((pos < src_len)) {
    if ((is_space(((long long)(*(uint8_t*)((uintptr_t)((src + pos)))))) == 0)) {
      return 0;
    }
    if ((((long long)(*(uint8_t*)((uintptr_t)((src + pos))))) == 10)) {
      cur_line = (cur_line + 1);
    }
    pos = (pos + 1);
  }
  return 0;
  return 0;
}

uint8_t check_kw(long long start, long long len) {
  if ((len == 2)) {
    if (buf_eq((src + start), (long long)"if", 2)) {
      return 1;
    }
  }
  if ((len == 3)) {
    if (buf_eq((src + start), (long long)"int", 3)) {
      return 1;
    }
    if (buf_eq((src + start), (long long)"ptr", 3)) {
      return 1;
    }
    if (buf_eq((src + start), (long long)"var", 3)) {
      return 1;
    }
  }
  if ((len == 4)) {
    if (buf_eq((src + start), (long long)"auto", 4)) {
      return 1;
    }
    if (buf_eq((src + start), (long long)"else", 4)) {
      return 1;
    }
    if (buf_eq((src + start), (long long)"char", 4)) {
      return 1;
    }
    if (buf_eq((src + start), (long long)"void", 4)) {
      return 1;
    }
    if (buf_eq((src + start), (long long)"byte", 4)) {
      return 1;
    }
    if (buf_eq((src + start), (long long)"half", 4)) {
      return 1;
    }
    if (buf_eq((src + start), (long long)"long", 4)) {
      return 1;
    }
    if (buf_eq((src + start), (long long)"enum", 4)) {
      return 1;
    }
  }
  if ((len == 5)) {
    if (buf_eq((src + start), (long long)"extrn", 5)) {
      return 1;
    }
    if (buf_eq((src + start), (long long)"while", 5)) {
      return 1;
    }
    if (buf_eq((src + start), (long long)"break", 5)) {
      return 1;
    }
  }
  if ((len == 6)) {
    if (buf_eq((src + start), (long long)"return", 6)) {
      return 1;
    }
    if (buf_eq((src + start), (long long)"import", 6)) {
      return 2;
    }
    if (buf_eq((src + start), (long long)"struct", 6)) {
      return 1;
    }
    if (buf_eq((src + start), (long long)"sizeof", 6)) {
      return 1;
    }
  }
  return 0;
  return 0;
}

uint8_t scan_comment(void) {
  long long next;
  if (((pos + 1) >= src_len)) {
    return 0;
  }
  next = ((long long)(*(uint8_t*)((uintptr_t)(((src + pos) + 1)))));
  if ((next == 47)) {
    pos = (pos + 2);
    while ((pos < src_len)) {
      if ((((long long)(*(uint8_t*)((uintptr_t)((src + pos))))) == 10)) {
        pos = (pos + 1);
        return 1;
      }
      pos = (pos + 1);
    }
    return 1;
  }
  if ((next == 42)) {
    pos = (pos + 2);
    while (((pos + 1) < src_len)) {
      if ((((long long)(*(uint8_t*)((uintptr_t)((src + pos))))) == 42)) {
        if ((((long long)(*(uint8_t*)((uintptr_t)(((src + pos) + 1))))) == 47)) {
          pos = (pos + 2);
          return 1;
        }
      }
      pos = (pos + 1);
    }
    return 1;
  }
  return 0;
  return 0;
}

uint8_t scan_word(void) {
  long long start, len, kw;
  start = pos;
  while ((pos < src_len)) {
    if ((is_alnum(((long long)(*(uint8_t*)((uintptr_t)((src + pos)))))) == 0)) {
      len = (pos - start);
      kw = check_kw(start, len);
      if ((kw == 2)) {
        add_token(1, start, len);
      } else {
        if ((kw == 1)) {
          add_token(0, start, len);
        } else {
          add_token(2, start, len);
        }
      }
      return 0;
    }
    pos = (pos + 1);
  }
  len = (pos - start);
  if ((len > 0)) {
    kw = check_kw(start, len);
    if ((kw == 2)) {
      add_token(1, start, len);
    } else {
      if ((kw == 1)) {
        add_token(0, start, len);
      } else {
        add_token(2, start, len);
      }
    }
  }
  return 0;
  return 0;
}

uint8_t scan_number(void) {
  long long start;
  start = pos;
  if ((((long long)(*(uint8_t*)((uintptr_t)((src + pos))))) == 48)) {
    if (((pos + 1) < src_len)) {
      if (((((long long)(*(uint8_t*)((uintptr_t)(((src + pos) + 1))))) == 120) | (((long long)(*(uint8_t*)((uintptr_t)(((src + pos) + 1))))) == 88))) {
        pos = (pos + 2);
        while ((pos < src_len)) {
          if ((is_hex(((long long)(*(uint8_t*)((uintptr_t)((src + pos)))))) == 0)) {
            add_token(4, start, (pos - start));
            return 0;
          }
          pos = (pos + 1);
        }
        add_token(4, start, (pos - start));
        return 0;
      }
    }
  }
  while ((pos < src_len)) {
    if ((is_digit(((long long)(*(uint8_t*)((uintptr_t)((src + pos)))))) == 0)) {
      if ((((long long)(*(uint8_t*)((uintptr_t)((src + pos))))) == 46)) {
        if (((pos + 1) < src_len)) {
          if (is_digit(((long long)(*(uint8_t*)((uintptr_t)(((src + pos) + 1))))))) {
            pos = (pos + 1);
            while ((pos < src_len)) {
              if ((is_digit(((long long)(*(uint8_t*)((uintptr_t)((src + pos)))))) == 0)) {
                add_token(8, start, (pos - start));
                return 0;
              }
              pos = (pos + 1);
            }
            add_token(8, start, (pos - start));
            return 0;
          }
        }
      }
      add_token(3, start, (pos - start));
      return 0;
    }
    pos = (pos + 1);
  }
  add_token(3, start, (pos - start));
  return 0;
  return 0;
}

uint8_t scan_string(void) {
  long long start;
  start = pos;
  pos = (pos + 1);
  while ((pos < src_len)) {
    if ((((long long)(*(uint8_t*)((uintptr_t)((src + pos))))) == 34)) {
      pos = (pos + 1);
      add_token(5, start, (pos - start));
      return 0;
    }
    pos = (pos + 1);
  }
  diag_warn(1);
  diag_str((long long)"unterminated string literal at line ");
  diag_int(cur_line);
  diag_newline();
  add_token(5, start, (pos - start));
  return 0;
  return 0;
}

uint8_t scan_char(void) {
  long long ch, val, tmp, digits, dcnt, j, vstart, vlen, base, char_start;
  char_start = pos;
  pos = (pos + 1);
  ch = ((long long)(*(uint8_t*)((uintptr_t)((src + pos)))));
  if ((ch == 92)) {
    pos = (pos + 1);
    ch = ((long long)(*(uint8_t*)((uintptr_t)((src + pos)))));
    if ((ch == 110)) {
      val = 10;
    } else {
      if ((ch == 116)) {
        val = 9;
      } else {
        if ((ch == 114)) {
          val = 13;
        } else {
          if ((ch == 92)) {
            val = 92;
          } else {
            if ((ch == 39)) {
              val = 39;
            } else {
              val = ch;
            }
          }
        }
      }
    }
  } else {
    val = ch;
  }
  pos = (pos + 1);
  pos = (pos + 1);
  digits = (long long)malloc(8);
  vstart = vbuf_pos;
  if ((val == 0)) {
    (*(uint8_t*)((uintptr_t)((vbuf + vbuf_pos))) = (uint8_t)(48));
    vbuf_pos = (vbuf_pos + 1);
  } else {
    tmp = val;
    dcnt = 0;
    while ((tmp > 0)) {
      (*(uint8_t*)((uintptr_t)((digits + dcnt))) = (uint8_t)((48 + (tmp % 10))));
      tmp = (tmp / 10);
      dcnt = (dcnt + 1);
    }
    j = (dcnt - 1);
    while ((j >= 0)) {
      (*(uint8_t*)((uintptr_t)((vbuf + vbuf_pos))) = (uint8_t)(((long long)(*(uint8_t*)((uintptr_t)((digits + j)))))));
      vbuf_pos = (vbuf_pos + 1);
      j = (j - 1);
    }
  }
  vlen = (vbuf_pos - vstart);
  base = (toks + (tok_cnt * 24));
  *(long long*)((uintptr_t)(base)) = 3;
  *(long long*)((uintptr_t)((base + 8))) = vstart;
  *(long long*)((uintptr_t)((base + 16))) = vlen;
  *(long long*)((uintptr_t)((tok_lines + (tok_cnt * 8)))) = cur_line;
  *(long long*)((uintptr_t)((tok_src_off + (tok_cnt * 8)))) = char_start;
  tok_cnt = (tok_cnt + 1);
  return 0;
  return 0;
}

uint8_t scan_op(void) {
  long long start, ch, next;
  start = pos;
  ch = ((long long)(*(uint8_t*)((uintptr_t)((src + pos)))));
  pos = (pos + 1);
  if ((pos < src_len)) {
    next = ((long long)(*(uint8_t*)((uintptr_t)((src + pos)))));
    if ((next == 61)) {
      if (((((ch == 61) | (ch == 33)) | (ch == 60)) | (ch == 62))) {
        pos = (pos + 1);
      }
      if ((((((ch == 43) | (ch == 45)) | (ch == 42)) | (ch == 47)) | (ch == 37))) {
        pos = (pos + 1);
      }
    }
    if ((ch == 38)) {
      if ((next == 38)) {
        pos = (pos + 1);
      }
    }
    if ((ch == 124)) {
      if ((next == 124)) {
        pos = (pos + 1);
      }
    }
    if ((ch == 62)) {
      if ((next == 62)) {
        pos = (pos + 1);
      }
    }
    if ((ch == 60)) {
      if ((next == 60)) {
        pos = (pos + 1);
      }
    }
  }
  add_token(6, start, (pos - start));
  return 0;
  return 0;
}

uint8_t scan_punct(void) {
  add_token(7, pos, 1);
  pos = (pos + 1);
  return 0;
  return 0;
}

uint8_t scan_token(void) {
  long long ch;
  ch = ((long long)(*(uint8_t*)((uintptr_t)((src + pos)))));
  if ((ch == 47)) {
    if (scan_comment()) {
      return 0;
    }
    scan_op();
    return 0;
  }
  if ((is_alpha(ch) | (ch == 95))) {
    scan_word();
    return 0;
  }
  if (is_digit(ch)) {
    scan_number();
    return 0;
  }
  if ((ch == 34)) {
    scan_string();
    return 0;
  }
  if ((ch == 39)) {
    scan_char();
    return 0;
  }
  if (is_op(ch)) {
    scan_op();
    return 0;
  }
  if (is_punct(ch)) {
    scan_punct();
    return 0;
  }
  pos = (pos + 1);
  return 0;
  return 0;
}

uint8_t init_lexer(void) {
  pos = 0;
  cur_line = 1;
  tok_lines = (long long)malloc(1048576);
  tok_src_off = (long long)malloc(1048576);
  return 0;
  return 0;
}

uint8_t lex_all(void) {
  pos = 0;
  while ((pos < src_len)) {
    skip_ws();
    if ((pos < src_len)) {
      scan_token();
    }
  }
  return 0;
  return 0;
}

uint8_t lex_module(long long mi) {
  long long mod_start, mod_end, mc;
  mc = arr_len(diag_file_starts);
  mod_start = arr_get(diag_file_starts, mi);
  if (((mi + 1) < mc)) {
    mod_end = arr_get(diag_file_starts, (mi + 1));
  } else {
    mod_end = src_len;
  }
  pos = mod_start;
  while ((pos < mod_end)) {
    skip_ws();
    if ((pos < mod_end)) {
      scan_token();
    }
  }
  return 0;
  return 0;
}

uint8_t init_types(void) {
  ty_vt_names = arr_new();
  ty_vt_types = arr_new();
  ft_names = arr_new();
  ft_types = arr_new();
  fn_vt_data = arr_new();
  fn_vt_cnts = arr_new();
  fn_ret_types = arr_new();
  fn_par_types = arr_new();
  ty_cur_ret = TY_UNKNOWN;
  ty_gl_names = arr_new();
  ty_gl_types = arr_new();
  return 0;
  return 0;
}

uint8_t ty_set_global_type(long long name_p, long long ty) {
  long long i;
  i = 0;
  while ((i < arr_len(ty_gl_names))) {
    if (packed_eq(arr_get(ty_gl_names, i), name_p)) {
      arr_set(ty_gl_types, i, promote(arr_get(ty_gl_types, i), ty));
      return 0;
    }
    i = (i + 1);
  }
  ty_gl_names = arr_push(ty_gl_names, name_p);
  ty_gl_types = arr_push(ty_gl_types, ty);
  return 0;
  return 0;
}

long long ty_get_global_type(long long name_p) {
  long long i;
  i = 0;
  while ((i < arr_len(ty_gl_names))) {
    if (packed_eq(arr_get(ty_gl_names, i), name_p)) {
      return arr_get(ty_gl_types, i);
    }
    i = (i + 1);
  }
  return TY_WORD;
  return 0;
}

uint8_t ty_reset_scope(void) {
  arr_clear(ty_vt_names);
  arr_clear(ty_vt_types);
  return 0;
  return 0;
}

long long ty_var_type(long long name_p) {
  long long i;
  i = 0;
  while ((i < arr_len(ty_vt_names))) {
    if (packed_eq(arr_get(ty_vt_names, i), name_p)) {
      return arr_get(ty_vt_types, i);
    }
    i = (i + 1);
  }
  return TY_UNKNOWN;
  return 0;
}

uint8_t ty_set_var_type(long long name_p, long long ty) {
  long long i, old;
  i = 0;
  while ((i < arr_len(ty_vt_names))) {
    if (packed_eq(arr_get(ty_vt_names, i), name_p)) {
      old = arr_get(ty_vt_types, i);
      if ((ty == TY_FLOAT)) {
        arr_set(ty_vt_types, i, TY_FLOAT);
      } else {
        if ((ty > old)) {
          arr_set(ty_vt_types, i, ty);
        }
      }
      return 0;
    }
    i = (i + 1);
  }
  ty_vt_names = arr_push(ty_vt_names, name_p);
  ty_vt_types = arr_push(ty_vt_types, ty);
  return 0;
  return 0;
}

uint8_t set_func_type(long long name_p, long long ty) {
  long long i;
  i = 0;
  while ((i < arr_len(ft_names))) {
    if (packed_eq(arr_get(ft_names, i), name_p)) {
      arr_set(ft_types, i, ty);
      return 0;
    }
    i = (i + 1);
  }
  ft_names = arr_push(ft_names, name_p);
  ft_types = arr_push(ft_types, ty);
  return 0;
  return 0;
}

long long func_type(long long name_p) {
  long long i;
  i = 0;
  while ((i < arr_len(ft_names))) {
    if (packed_eq(arr_get(ft_names, i), name_p)) {
      return arr_get(ft_types, i);
    }
    i = (i + 1);
  }
  return TY_WORD;
  return 0;
}

long long promote(long long a, long long b) {
  if ((a == TY_PTR)) {
    return TY_PTR;
  }
  if ((b == TY_PTR)) {
    return TY_PTR;
  }
  if ((a == TY_FLOAT)) {
    return TY_FLOAT;
  }
  if ((b == TY_FLOAT)) {
    return TY_FLOAT;
  }
  if ((a > b)) {
    return a;
  }
  return b;
  return 0;
}

long long type_of_lit(long long vstart, long long vlen) {
  long long ch, val, i, is_hex;
  ch = ((long long)(*(uint8_t*)((uintptr_t)(vstart))));
  if ((ch == 34)) {
    return TY_PTR;
  }
  i = 0;
  while ((i < vlen)) {
    if ((((long long)(*(uint8_t*)((uintptr_t)((vstart + i))))) == 46)) {
      return TY_FLOAT;
    }
    i = (i + 1);
  }
  val = 0;
  is_hex = 0;
  if ((vlen > 2)) {
    if ((((long long)(*(uint8_t*)((uintptr_t)(vstart)))) == 48)) {
      if ((((long long)(*(uint8_t*)((uintptr_t)((vstart + 1))))) == 120)) {
        is_hex = 1;
      }
    }
  }
  if (is_hex) {
    i = 2;
    while ((i < vlen)) {
      ch = ((long long)(*(uint8_t*)((uintptr_t)((vstart + i)))));
      val = (val * 16);
      if ((ch >= 48)) {
        if ((ch <= 57)) {
          val = ((val + ch) - 48);
        }
      }
      if ((ch >= 65)) {
        if ((ch <= 70)) {
          val = ((val + ch) - 55);
        }
      }
      if ((ch >= 97)) {
        if ((ch <= 102)) {
          val = ((val + ch) - 87);
        }
      }
      i = (i + 1);
    }
  } else {
    i = 0;
    while ((i < vlen)) {
      val = (((val * 10) + ((long long)(*(uint8_t*)((uintptr_t)((vstart + i)))))) - 48);
      i = (i + 1);
    }
  }
  if ((val <= 255)) {
    return TY_BYTE;
  }
  if ((val <= 65535)) {
    return TY_QUART;
  }
  if ((val <= 0xFFFFFFFF)) {
    return TY_HALF;
  }
  return TY_WORD;
  return 0;
}

long long add_type(long long nd) {
  long long t, ty, lt, rt, name_p, arr, cnt, hint, i;
  long long n, lhs;
  if ((nd == 0)) {
    return TY_UNKNOWN;
  }
  n = nd;
  t = (*(long long*)((uintptr_t)((n + 0))));
  if ((t == T_LIT)) {
    ty = type_of_lit((vbuf + unpack_s((*(long long*)((uintptr_t)((n + 8)))))), unpack_l((*(long long*)((uintptr_t)((n + 8))))));
    *(long long*)((uintptr_t)((n + 56))) = ty;
    return ty;
  }
  if ((t == T_VAR)) {
    ty = ty_var_type((*(long long*)((uintptr_t)((n + 8)))));
    *(long long*)((uintptr_t)((n + 56))) = ty;
    return ty;
  }
  if ((t == T_BINOP)) {
    lt = add_type((*(long long*)((uintptr_t)((n + 16)))));
    rt = add_type((*(long long*)((uintptr_t)((n + 24)))));
    ty = promote(lt, rt);
    *(long long*)((uintptr_t)((n + 56))) = ty;
    return ty;
  }
  if ((t == T_UNARY)) {
    ty = add_type((*(long long*)((uintptr_t)((n + 16)))));
    *(long long*)((uintptr_t)((n + 56))) = ty;
    return ty;
  }
  if ((t == T_ASSIGN)) {
    rt = add_type((*(long long*)((uintptr_t)((n + 16)))));
    add_type((*(long long*)((uintptr_t)((n + 8)))));
    lhs = (*(long long*)((uintptr_t)((n + 8))));
    if (((*(long long*)((uintptr_t)((lhs + 0)))) == T_VAR)) {
      ty_set_var_type((*(long long*)((uintptr_t)((lhs + 8)))), rt);
      if ((rt == TY_FLOAT)) {
        ty_set_global_type((*(long long*)((uintptr_t)((lhs + 8)))), TY_FLOAT);
      }
    }
    *(long long*)((uintptr_t)((n + 56))) = rt;
    return rt;
  }
  if ((t == T_MEMLD)) {
    add_type((*(long long*)((uintptr_t)((n + 8)))));
    *(long long*)((uintptr_t)((n + 56))) = TY_WORD;
    return TY_WORD;
  }
  if ((t == T_MEMST)) {
    add_type((*(long long*)((uintptr_t)((n + 8)))));
    add_type((*(long long*)((uintptr_t)((n + 16)))));
    *(long long*)((uintptr_t)((n + 56))) = TY_UNKNOWN;
    return TY_UNKNOWN;
  }
  if ((t == T_CALL)) {
    add_type_list((*(long long*)((uintptr_t)((n + 16)))), (*(long long*)((uintptr_t)((n + 24)))));
    ty = func_type((*(long long*)((uintptr_t)((n + 8)))));
    *(long long*)((uintptr_t)((n + 56))) = ty;
    return ty;
  }
  if ((t == T_RET)) {
    ty = add_type((*(long long*)((uintptr_t)((n + 8)))));
    *(long long*)((uintptr_t)((n + 56))) = ty;
    ty_cur_ret = promote(ty_cur_ret, ty);
    return ty;
  }
  if ((t == T_DECL)) {
    long long hints_arr;
    hints_arr = (*(long long*)((uintptr_t)((n + 40))));
    arr = (*(long long*)((uintptr_t)((n + 8))));
    cnt = (*(long long*)((uintptr_t)((n + 16))));
    if ((hints_arr != 0)) {
      i = 0;
      while ((i < cnt)) {
        hint = (*(long long*)((uintptr_t)((hints_arr + (i * 8)))));
        if ((hint > 0)) {
          ty_set_var_type((*(long long*)((uintptr_t)((arr + (i * 8))))), hint);
        }
        i = (i + 1);
      }
    } else {
      hint = (*(long long*)((uintptr_t)((n + 24))));
      if ((hint > 0)) {
        i = 0;
        while ((i < cnt)) {
          ty_set_var_type((*(long long*)((uintptr_t)((arr + (i * 8))))), hint);
          i = (i + 1);
        }
      }
    }
    *(long long*)((uintptr_t)((n + 56))) = TY_UNKNOWN;
    return TY_UNKNOWN;
  }
  if ((t == T_IF)) {
    add_type((*(long long*)((uintptr_t)((n + 8)))));
    add_type_list((*(long long*)((uintptr_t)((n + 16)))), (*(long long*)((uintptr_t)((n + 24)))));
    if (((*(long long*)((uintptr_t)((n + 40)))) > 0)) {
      add_type_list((*(long long*)((uintptr_t)((n + 32)))), (*(long long*)((uintptr_t)((n + 40)))));
    }
    *(long long*)((uintptr_t)((n + 56))) = TY_UNKNOWN;
    return TY_UNKNOWN;
  }
  if ((t == T_WHILE)) {
    add_type((*(long long*)((uintptr_t)((n + 8)))));
    add_type_list((*(long long*)((uintptr_t)((n + 16)))), (*(long long*)((uintptr_t)((n + 24)))));
    *(long long*)((uintptr_t)((n + 56))) = TY_UNKNOWN;
    return TY_UNKNOWN;
  }
  *(long long*)((uintptr_t)((n + 56))) = TY_UNKNOWN;
  return TY_UNKNOWN;
  return 0;
}

uint8_t add_type_list(long long arr, long long cnt) {
  long long i;
  i = 0;
  while ((i < cnt)) {
    add_type((*(long long*)((uintptr_t)((arr + (i * 8))))));
    i = (i + 1);
  }
  return 0;
  return 0;
}

uint8_t save_fn_types(long long func_idx) {
  long long block, i, cnt;
  cnt = arr_len(ty_vt_names);
  block = (long long)malloc((cnt * 16));
  i = 0;
  while ((i < cnt)) {
    *(long long*)((uintptr_t)((block + (i * 16)))) = arr_get(ty_vt_names, i);
    *(long long*)((uintptr_t)(((block + (i * 16)) + 8))) = arr_get(ty_vt_types, i);
    i = (i + 1);
  }
  arr_set(fn_vt_data, func_idx, block);
  arr_set(fn_vt_cnts, func_idx, cnt);
  arr_set(fn_ret_types, func_idx, ty_cur_ret);
  return 0;
  return 0;
}

uint8_t save_fn_par_types(long long func_idx, long long params_arr, long long param_cnt) {
  long long block, i;
  block = (long long)malloc((param_cnt * 8));
  i = 0;
  while ((i < param_cnt)) {
    *(long long*)((uintptr_t)((block + (i * 8)))) = ty_var_type((*(long long*)((uintptr_t)((params_arr + (i * 8))))));
    i = (i + 1);
  }
  arr_set(fn_par_types, func_idx, block);
  return 0;
  return 0;
}

long long get_fn_var_type(long long func_idx, long long name_p) {
  long long block, cnt, i;
  block = arr_get(fn_vt_data, func_idx);
  cnt = arr_get(fn_vt_cnts, func_idx);
  i = 0;
  while ((i < cnt)) {
    if (packed_eq((*(long long*)((uintptr_t)((block + (i * 16))))), name_p)) {
      return (*(long long*)((uintptr_t)(((block + (i * 16)) + 8))));
    }
    i = (i + 1);
  }
  return TY_WORD;
  return 0;
}

long long get_fn_ret_type(long long func_idx) {
  return arr_get(fn_ret_types, func_idx);
  return 0;
}

long long get_fn_par_type(long long func_idx, long long par_idx) {
  long long block;
  block = arr_get(fn_par_types, func_idx);
  if ((block == 0)) {
    return TY_WORD;
  }
  return (*(long long*)((uintptr_t)((block + (par_idx * 8)))));
  return 0;
}

long long infer_one_func(long long func_idx) {
  long long func, name_p, params_arr, param_cnt, body_arr, body_cnt, j, pt;
  long long hints, h;
  func = (*(long long*)((uintptr_t)((funcs + (func_idx * 8)))));
  name_p = (*(long long*)((uintptr_t)((func + FN_NAME))));
  params_arr = (*(long long*)((uintptr_t)((func + FN_PARS))));
  param_cnt = (*(long long*)((uintptr_t)((func + FN_PCNT))));
  body_arr = (*(long long*)((uintptr_t)((func + FN_BODY))));
  body_cnt = (*(long long*)((uintptr_t)((func + FN_BCNT))));
  hints = (*(long long*)((uintptr_t)((func + FN_HINTS))));
  ty_reset_scope();
  ty_cur_ret = TY_UNKNOWN;
  j = 0;
  while ((j < param_cnt)) {
    pt = get_fn_par_type(func_idx, j);
    if ((hints != 0)) {
      h = (*(long long*)((uintptr_t)((hints + (j * 8)))));
      if ((h > 0)) {
        pt = h;
      }
    }
    if ((pt == TY_UNKNOWN)) {
      pt = TY_WORD;
    }
    ty_set_var_type((*(long long*)((uintptr_t)((params_arr + (j * 8))))), pt);
    j = (j + 1);
  }
  add_type_list(body_arr, body_cnt);
  save_fn_types(func_idx);
  save_fn_par_types(func_idx, params_arr, param_cnt);
  return ty_cur_ret;
  return 0;
}

long long propagate_call_params(void) {
  long long changed, i, func, body_arr, body_cnt;
  changed = 0;
  i = 0;
  while ((i < func_cnt)) {
    func = (*(long long*)((uintptr_t)((funcs + (i * 8)))));
    body_arr = (*(long long*)((uintptr_t)((func + FN_BODY))));
    body_cnt = (*(long long*)((uintptr_t)((func + FN_BCNT))));
    changed = (changed + propagate_in_body(body_arr, body_cnt, i));
    i = (i + 1);
  }
  return changed;
  return 0;
}

long long propagate_in_body(long long arr, long long cnt, long long caller_idx) {
  long long i, nd, changed;
  long long n;
  changed = 0;
  i = 0;
  while ((i < cnt)) {
    nd = (*(long long*)((uintptr_t)((arr + (i * 8)))));
    if ((nd != 0)) {
      changed = (changed + propagate_in_node(nd, caller_idx));
    }
    i = (i + 1);
  }
  return changed;
  return 0;
}

long long propagate_in_node(long long nd, long long caller_idx) {
  long long n, t, changed, name_p, args, cnt, j, fi, arg_ty;
  long long an;
  long long callee_func, callee_params, callee_pcnt, callee_body, callee_bcnt, par_p;
  if ((nd == 0)) {
    return 0;
  }
  n = nd;
  t = (*(long long*)((uintptr_t)((n + 0))));
  changed = 0;
  if ((t == T_CALL)) {
    name_p = (*(long long*)((uintptr_t)((n + 8))));
    args = (*(long long*)((uintptr_t)((n + 16))));
    cnt = (*(long long*)((uintptr_t)((n + 24))));
    fi = find_func_idx(name_p);
    if ((fi >= 0)) {
      callee_func = (*(long long*)((uintptr_t)((funcs + (fi * 8)))));
      callee_params = (*(long long*)((uintptr_t)((callee_func + FN_PARS))));
      callee_pcnt = (*(long long*)((uintptr_t)((callee_func + FN_PCNT))));
      callee_body = (*(long long*)((uintptr_t)((callee_func + FN_BODY))));
      callee_bcnt = (*(long long*)((uintptr_t)((callee_func + FN_BCNT))));
      j = 0;
      while ((j < cnt)) {
        an = (*(long long*)((uintptr_t)((args + (j * 8)))));
        if ((an != 0)) {
          arg_ty = (*(long long*)((uintptr_t)((an + 56))));
          if ((arg_ty == TY_FLOAT)) {
            if ((get_fn_par_type(fi, j) != TY_FLOAT)) {
              if ((j < callee_pcnt)) {
                par_p = (*(long long*)((uintptr_t)((callee_params + (j * 8)))));
                if ((uses_int_ops_list(callee_body, callee_bcnt, par_p) == 0)) {
                  propagate_set_par_type(fi, j, TY_FLOAT);
                  changed = 1;
                }
              }
            }
          }
        }
        j = (j + 1);
      }
    }
    j = 0;
    while ((j < cnt)) {
      changed = (changed + propagate_in_node((*(long long*)((uintptr_t)((args + (j * 8))))), caller_idx));
      j = (j + 1);
    }
    return changed;
  }
  if ((t == T_ASSIGN)) {
    changed = (changed + propagate_in_node((*(long long*)((uintptr_t)((n + 8)))), caller_idx));
    changed = (changed + propagate_in_node((*(long long*)((uintptr_t)((n + 16)))), caller_idx));
    return changed;
  }
  if ((t == T_BINOP)) {
    changed = (changed + propagate_in_node((*(long long*)((uintptr_t)((n + 16)))), caller_idx));
    changed = (changed + propagate_in_node((*(long long*)((uintptr_t)((n + 24)))), caller_idx));
    return changed;
  }
  if ((t == T_UNARY)) {
    changed = (changed + propagate_in_node((*(long long*)((uintptr_t)((n + 16)))), caller_idx));
    return changed;
  }
  if ((t == T_RET)) {
    changed = (changed + propagate_in_node((*(long long*)((uintptr_t)((n + 8)))), caller_idx));
    return changed;
  }
  if ((t == T_IF)) {
    changed = (changed + propagate_in_node((*(long long*)((uintptr_t)((n + 8)))), caller_idx));
    changed = (changed + propagate_in_body((*(long long*)((uintptr_t)((n + 16)))), (*(long long*)((uintptr_t)((n + 24)))), caller_idx));
    if (((*(long long*)((uintptr_t)((n + 40)))) > 0)) {
      changed = (changed + propagate_in_body((*(long long*)((uintptr_t)((n + 32)))), (*(long long*)((uintptr_t)((n + 40)))), caller_idx));
    }
    return changed;
  }
  if ((t == T_WHILE)) {
    changed = (changed + propagate_in_node((*(long long*)((uintptr_t)((n + 8)))), caller_idx));
    changed = (changed + propagate_in_body((*(long long*)((uintptr_t)((n + 16)))), (*(long long*)((uintptr_t)((n + 24)))), caller_idx));
    return changed;
  }
  if ((t == T_MEMLD)) {
    changed = (changed + propagate_in_node((*(long long*)((uintptr_t)((n + 8)))), caller_idx));
    return changed;
  }
  if ((t == T_MEMST)) {
    changed = (changed + propagate_in_node((*(long long*)((uintptr_t)((n + 8)))), caller_idx));
    changed = (changed + propagate_in_node((*(long long*)((uintptr_t)((n + 16)))), caller_idx));
    return changed;
  }
  return changed;
  return 0;
}

uint8_t is_int_only_op(long long op_p) {
  long long s, l;
  s = unpack_s(op_p);
  l = unpack_l(op_p);
  if ((l == 1)) {
    if ((((long long)(*(uint8_t*)((uintptr_t)((vbuf + s))))) == 37)) {
      return 1;
    }
    if ((((long long)(*(uint8_t*)((uintptr_t)((vbuf + s))))) == 38)) {
      return 1;
    }
    if ((((long long)(*(uint8_t*)((uintptr_t)((vbuf + s))))) == 124)) {
      return 1;
    }
    if ((((long long)(*(uint8_t*)((uintptr_t)((vbuf + s))))) == 94)) {
      return 1;
    }
  }
  if ((l == 2)) {
    if ((((long long)(*(uint8_t*)((uintptr_t)((vbuf + s))))) == 60)) {
      if ((((long long)(*(uint8_t*)((uintptr_t)(((vbuf + s) + 1))))) == 60)) {
        return 1;
      }
    }
    if ((((long long)(*(uint8_t*)((uintptr_t)((vbuf + s))))) == 62)) {
      if ((((long long)(*(uint8_t*)((uintptr_t)(((vbuf + s) + 1))))) == 62)) {
        return 1;
      }
    }
  }
  return 0;
  return 0;
}

uint8_t uses_int_ops_list(long long arr, long long cnt, long long var_p) {
  long long i, nd;
  i = 0;
  while ((i < cnt)) {
    nd = (*(long long*)((uintptr_t)((arr + (i * 8)))));
    if ((nd != 0)) {
      if (uses_int_ops_node(nd, var_p)) {
        return 1;
      }
    }
    i = (i + 1);
  }
  return 0;
  return 0;
}

uint8_t uses_int_ops_node(long long nd, long long var_p) {
  long long n, t, lhs, rhs;
  if ((nd == 0)) {
    return 0;
  }
  n = nd;
  t = (*(long long*)((uintptr_t)((n + 0))));
  if ((t == T_BINOP)) {
    if (is_int_only_op((*(long long*)((uintptr_t)((n + 8)))))) {
      lhs = (*(long long*)((uintptr_t)((n + 16))));
      rhs = (*(long long*)((uintptr_t)((n + 24))));
      if ((lhs != 0)) {
        if (((*(long long*)((uintptr_t)((lhs + 0)))) == T_VAR)) {
          if (packed_eq((*(long long*)((uintptr_t)((lhs + 8)))), var_p)) {
            return 1;
          }
        }
      }
      if ((rhs != 0)) {
        if (((*(long long*)((uintptr_t)((rhs + 0)))) == T_VAR)) {
          if (packed_eq((*(long long*)((uintptr_t)((rhs + 8)))), var_p)) {
            return 1;
          }
        }
      }
    }
    if (uses_int_ops_node((*(long long*)((uintptr_t)((n + 16)))), var_p)) {
      return 1;
    }
    if (uses_int_ops_node((*(long long*)((uintptr_t)((n + 24)))), var_p)) {
      return 1;
    }
    return 0;
  }
  if ((t == T_ASSIGN)) {
    if (uses_int_ops_node((*(long long*)((uintptr_t)((n + 8)))), var_p)) {
      return 1;
    }
    if (uses_int_ops_node((*(long long*)((uintptr_t)((n + 16)))), var_p)) {
      return 1;
    }
    return 0;
  }
  if ((t == T_UNARY)) {
    if (uses_int_ops_node((*(long long*)((uintptr_t)((n + 16)))), var_p)) {
      return 1;
    }
    return 0;
  }
  if ((t == T_RET)) {
    if (uses_int_ops_node((*(long long*)((uintptr_t)((n + 8)))), var_p)) {
      return 1;
    }
    return 0;
  }
  if ((t == T_CALL)) {
    return uses_int_ops_list((*(long long*)((uintptr_t)((n + 16)))), (*(long long*)((uintptr_t)((n + 24)))), var_p);
  }
  if ((t == T_IF)) {
    if (uses_int_ops_node((*(long long*)((uintptr_t)((n + 8)))), var_p)) {
      return 1;
    }
    if (uses_int_ops_list((*(long long*)((uintptr_t)((n + 16)))), (*(long long*)((uintptr_t)((n + 24)))), var_p)) {
      return 1;
    }
    if (((*(long long*)((uintptr_t)((n + 40)))) > 0)) {
      if (uses_int_ops_list((*(long long*)((uintptr_t)((n + 32)))), (*(long long*)((uintptr_t)((n + 40)))), var_p)) {
        return 1;
      }
    }
    return 0;
  }
  if ((t == T_WHILE)) {
    if (uses_int_ops_node((*(long long*)((uintptr_t)((n + 8)))), var_p)) {
      return 1;
    }
    if (uses_int_ops_list((*(long long*)((uintptr_t)((n + 16)))), (*(long long*)((uintptr_t)((n + 24)))), var_p)) {
      return 1;
    }
    return 0;
  }
  if ((t == T_MEMLD)) {
    return uses_int_ops_node((*(long long*)((uintptr_t)((n + 8)))), var_p);
  }
  if ((t == T_MEMST)) {
    if (uses_int_ops_node((*(long long*)((uintptr_t)((n + 8)))), var_p)) {
      return 1;
    }
    if (uses_int_ops_node((*(long long*)((uintptr_t)((n + 16)))), var_p)) {
      return 1;
    }
    return 0;
  }
  return 0;
  return 0;
}

uint8_t find_func_idx(long long name_p) {
  long long i, func;
  i = 0;
  while ((i < func_cnt)) {
    func = (*(long long*)((uintptr_t)((funcs + (i * 8)))));
    if (packed_eq((*(long long*)((uintptr_t)((func + FN_NAME)))), name_p)) {
      return i;
    }
    i = (i + 1);
  }
  return (-1);
  return 0;
}

uint8_t propagate_set_par_type(long long func_idx, long long par_idx, long long ty) {
  long long block, pcnt, func, k;
  block = arr_get(fn_par_types, func_idx);
  if ((block == 0)) {
    func = arr_get(funcs, func_idx);
    pcnt = (*(long long*)((uintptr_t)((func + FN_PCNT))));
    block = (long long)malloc((pcnt * 8));
    arr_set(fn_par_types, func_idx, block);
    k = 0;
    while ((k < pcnt)) {
      *(long long*)((uintptr_t)((block + (k * 8)))) = TY_WORD;
      k = (k + 1);
    }
  }
  *(long long*)((uintptr_t)((block + (par_idx * 8)))) = ty;
  return 0;
  return 0;
}

uint8_t infer_module(long long mi) {
  long long i, func, name_p, ret_ty;
  i = 0;
  while ((i < ext_cnt)) {
    if ((arr_get(extern_mods, i) == mi)) {
      long long ret_p, rlen;
      func = (*(long long*)((uintptr_t)((externs + (i * 8)))));
      name_p = (*(long long*)((uintptr_t)((func + EX_NAME))));
      ret_p = (*(long long*)((uintptr_t)((func + EX_RET))));
      rlen = unpack_l(ret_p);
      if ((rlen == 6)) {
        if (buf_eq((vbuf + unpack_s(ret_p)), (long long)"double", 6)) {
          set_func_type(name_p, TY_FLOAT);
        } else {
          set_func_type(name_p, TY_WORD);
        }
      } else {
        set_func_type(name_p, TY_WORD);
      }
    }
    i = (i + 1);
  }
  while ((arr_len(fn_vt_data) < func_cnt)) {
    fn_vt_data = arr_push(fn_vt_data, 0);
    fn_vt_cnts = arr_push(fn_vt_cnts, 0);
    fn_ret_types = arr_push(fn_ret_types, 0);
    fn_par_types = arr_push(fn_par_types, 0);
  }
  i = 0;
  while ((i < func_cnt)) {
    if ((arr_get(func_mods, i) == mi)) {
      ret_ty = infer_one_func(i);
      func = (*(long long*)((uintptr_t)((funcs + (i * 8)))));
      name_p = (*(long long*)((uintptr_t)((func + FN_NAME))));
      if ((ret_ty == TY_UNKNOWN)) {
        ret_ty = TY_WORD;
      }
      set_func_type(name_p, ret_ty);
    }
    i = (i + 1);
  }
  return 0;
  return 0;
}

uint8_t run_types(void) {
  long long i, func, name_p, ret_ty, changed, pass;
  i = 0;
  while ((i < ext_cnt)) {
    long long ret_p, rlen;
    func = (*(long long*)((uintptr_t)((externs + (i * 8)))));
    name_p = (*(long long*)((uintptr_t)((func + EX_NAME))));
    ret_p = (*(long long*)((uintptr_t)((func + EX_RET))));
    rlen = unpack_l(ret_p);
    if ((rlen == 6)) {
      if (buf_eq((vbuf + unpack_s(ret_p)), (long long)"double", 6)) {
        set_func_type(name_p, TY_FLOAT);
      } else {
        set_func_type(name_p, TY_WORD);
      }
    } else {
      set_func_type(name_p, TY_WORD);
    }
    i = (i + 1);
  }
  arr_clear(fn_vt_data);
  arr_clear(fn_vt_cnts);
  arr_clear(fn_ret_types);
  arr_clear(fn_par_types);
  i = 0;
  while ((i < func_cnt)) {
    fn_vt_data = arr_push(fn_vt_data, 0);
    fn_vt_cnts = arr_push(fn_vt_cnts, 0);
    fn_ret_types = arr_push(fn_ret_types, 0);
    fn_par_types = arr_push(fn_par_types, 0);
    i = (i + 1);
  }
  pass = 0;
  changed = 1;
  while ((changed > 0)) {
    changed = 0;
    i = 0;
    while ((i < func_cnt)) {
      ret_ty = infer_one_func(i);
      func = (*(long long*)((uintptr_t)((funcs + (i * 8)))));
      name_p = (*(long long*)((uintptr_t)((func + FN_NAME))));
      if ((ret_ty == TY_UNKNOWN)) {
        ret_ty = TY_WORD;
      }
      set_func_type(name_p, ret_ty);
      i = (i + 1);
    }
    changed = 0;
    pass = (pass + 1);
    if ((pass >= 5)) {
      changed = 0;
    }
  }
  return 0;
  return 0;
}

uint8_t init_dispatch(void) {
  DSP_SEQ = 0;
  DSP_PAR = 1;
  DSP_GPU = 2;
  DSP_SPLIT = 3;
  pure_ext = arr_new();
  stride_vars = arr_new();
  dsp_local_vars = arr_new();
  return 0;
  return 0;
}

uint8_t add_pure_extern(long long name_p) {
  pure_ext = arr_push(pure_ext, name_p);
  return 0;
  return 0;
}

uint8_t is_pure_extern(long long name_p) {
  long long i;
  i = 0;
  while ((i < arr_len(pure_ext))) {
    if ((arr_get(pure_ext, i) == name_p)) {
      return 1;
    }
    i = (i + 1);
  }
  return 0;
  return 0;
}

uint8_t dsp_add_local(long long name_p) {
  dsp_local_vars = arr_push(dsp_local_vars, name_p);
  return 0;
  return 0;
}

uint8_t dsp_reset_locals(void) {
  arr_clear(dsp_local_vars);
  return 0;
  return 0;
}

uint8_t dsp_is_local(long long name_p) {
  long long i;
  i = 0;
  while ((i < arr_len(dsp_local_vars))) {
    if ((arr_get(dsp_local_vars, i) == name_p)) {
      return 1;
    }
    i = (i + 1);
  }
  return 0;
  return 0;
}

uint8_t is_strided(long long name_p) {
  long long i;
  i = 0;
  while ((i < arr_len(stride_vars))) {
    if ((arr_get(stride_vars, i) == name_p)) {
      return 1;
    }
    i = (i + 1);
  }
  return 0;
  return 0;
}

long long contains_var(long long nd, long long name_p) {
  long long n;
  long long t, i, arr, cnt;
  if ((nd == 0)) {
    return 0;
  }
  n = nd;
  t = (*(long long*)((uintptr_t)((n + 0))));
  if ((t == T_VAR)) {
    if (((*(long long*)((uintptr_t)((n + 8)))) == name_p)) {
      return 1;
    }
    return 0;
  }
  if ((t == T_BINOP)) {
    if (contains_var((*(long long*)((uintptr_t)((n + 16)))), name_p)) {
      return 1;
    }
    return contains_var((*(long long*)((uintptr_t)((n + 24)))), name_p);
  }
  if ((t == T_UNARY)) {
    return contains_var((*(long long*)((uintptr_t)((n + 16)))), name_p);
  }
  if ((t == T_CALL)) {
    arr = (*(long long*)((uintptr_t)((n + 16))));
    cnt = (*(long long*)((uintptr_t)((n + 24))));
    i = 0;
    while ((i < cnt)) {
      if (contains_var((*(long long*)((uintptr_t)((arr + (i * 8))))), name_p)) {
        return 1;
      }
      i = (i + 1);
    }
    return 0;
  }
  if ((t == T_MEMLD)) {
    return contains_var((*(long long*)((uintptr_t)((n + 8)))), name_p);
  }
  if ((t == T_ASSIGN)) {
    if (contains_var((*(long long*)((uintptr_t)((n + 8)))), name_p)) {
      return 1;
    }
    return contains_var((*(long long*)((uintptr_t)((n + 16)))), name_p);
  }
  return 0;
  return 0;
}

long long find_loop_var(long long body_arr, long long body_cnt) {
  long long i;
  long long stmt, left, right, binop_left;
  i = (body_cnt - 1);
  while ((i >= 0)) {
    stmt = (*(long long*)((uintptr_t)((body_arr + (i * 8)))));
    if ((stmt != 0)) {
      if (((*(long long*)((uintptr_t)((stmt + 0)))) == T_ASSIGN)) {
        left = (*(long long*)((uintptr_t)((stmt + 8))));
        right = (*(long long*)((uintptr_t)((stmt + 16))));
        if (((*(long long*)((uintptr_t)((left + 0)))) == T_VAR)) {
          if (((*(long long*)((uintptr_t)((right + 0)))) == T_BINOP)) {
            if (((*(long long*)((uintptr_t)((right + 8)))) == pack(43, 1))) {
              binop_left = (*(long long*)((uintptr_t)((right + 16))));
              if (((*(long long*)((uintptr_t)((binop_left + 0)))) == T_VAR)) {
                if (((*(long long*)((uintptr_t)((binop_left + 8)))) == (*(long long*)((uintptr_t)((left + 8)))))) {
                  return (*(long long*)((uintptr_t)((left + 8))));
                }
              }
            }
          }
        }
      }
    }
    i = (i - 1);
  }
  return 0;
  return 0;
}

uint8_t find_strided(long long body_arr, long long body_cnt, long long loop_var_p) {
  long long i;
  long long stmt, left, right;
  long long r_right;
  arr_clear(stride_vars);
  if ((loop_var_p == 0)) {
    return 0;
  }
  i = 0;
  while ((i < body_cnt)) {
    stmt = (*(long long*)((uintptr_t)((body_arr + (i * 8)))));
    if ((stmt != 0)) {
      if (((*(long long*)((uintptr_t)((stmt + 0)))) == T_ASSIGN)) {
        left = (*(long long*)((uintptr_t)((stmt + 8))));
        right = (*(long long*)((uintptr_t)((stmt + 16))));
        if (((*(long long*)((uintptr_t)((left + 0)))) == T_VAR)) {
          if (((*(long long*)((uintptr_t)((right + 0)))) == T_BINOP)) {
            r_right = (*(long long*)((uintptr_t)((right + 24))));
            if ((r_right != 0)) {
              if (((*(long long*)((uintptr_t)((r_right + 0)))) == T_BINOP)) {
                if (contains_var(r_right, loop_var_p)) {
                  stride_vars = arr_push(stride_vars, (*(long long*)((uintptr_t)((left + 8)))));
                }
              }
            }
          }
        }
      }
    }
    i = (i + 1);
  }
  return 0;
  return 0;
}

uint8_t has_side_effects(long long nd) {
  long long n, p, base;
  long long t, i, arr, cnt, name_p;
  if ((nd == 0)) {
    return 0;
  }
  n = nd;
  t = (*(long long*)((uintptr_t)((n + 0))));
  if ((t == T_CALL)) {
    name_p = (*(long long*)((uintptr_t)((n + 8))));
    if ((is_pure_extern(name_p) == 0)) {
      return 1;
    }
    arr = (*(long long*)((uintptr_t)((n + 16))));
    cnt = (*(long long*)((uintptr_t)((n + 24))));
    i = 0;
    while ((i < cnt)) {
      if (has_side_effects((*(long long*)((uintptr_t)((arr + (i * 8))))))) {
        return 1;
      }
      i = (i + 1);
    }
    return 0;
  }
  if ((t == T_ASSIGN)) {
    p = (*(long long*)((uintptr_t)((n + 8))));
    if (((*(long long*)((uintptr_t)((p + 0)))) == T_VAR)) {
      name_p = (*(long long*)((uintptr_t)((p + 8))));
      if ((dsp_is_local(name_p) == 0)) {
        if ((is_strided(name_p) == 0)) {
          return 1;
        }
      }
    }
    return has_side_effects((*(long long*)((uintptr_t)((n + 16)))));
  }
  if ((t == T_MEMST)) {
    p = (*(long long*)((uintptr_t)((n + 8))));
    if (((*(long long*)((uintptr_t)((p + 0)))) == T_BINOP)) {
      base = (*(long long*)((uintptr_t)((p + 16))));
      if (((*(long long*)((uintptr_t)((base + 0)))) == T_VAR)) {
        if (is_strided((*(long long*)((uintptr_t)((base + 8)))))) {
          return has_side_effects((*(long long*)((uintptr_t)((n + 16)))));
        }
      }
    }
    return 1;
  }
  if ((t == T_BINOP)) {
    if (has_side_effects((*(long long*)((uintptr_t)((n + 16)))))) {
      return 1;
    }
    return has_side_effects((*(long long*)((uintptr_t)((n + 24)))));
  }
  if ((t == T_UNARY)) {
    return has_side_effects((*(long long*)((uintptr_t)((n + 16)))));
  }
  if ((t == T_MEMLD)) {
    return has_side_effects((*(long long*)((uintptr_t)((n + 8)))));
  }
  if ((t == T_RET)) {
    return has_side_effects((*(long long*)((uintptr_t)((n + 8)))));
  }
  if ((t == T_IF)) {
    if (has_side_effects((*(long long*)((uintptr_t)((n + 8)))))) {
      return 1;
    }
    arr = (*(long long*)((uintptr_t)((n + 16))));
    cnt = (*(long long*)((uintptr_t)((n + 24))));
    i = 0;
    while ((i < cnt)) {
      if (has_side_effects((*(long long*)((uintptr_t)((arr + (i * 8))))))) {
        return 1;
      }
      i = (i + 1);
    }
    if (((*(long long*)((uintptr_t)((n + 40)))) > 0)) {
      arr = (*(long long*)((uintptr_t)((n + 32))));
      cnt = (*(long long*)((uintptr_t)((n + 40))));
      i = 0;
      while ((i < cnt)) {
        if (has_side_effects((*(long long*)((uintptr_t)((arr + (i * 8))))))) {
          return 1;
        }
        i = (i + 1);
      }
    }
    return 0;
  }
  if ((t == T_WHILE)) {
    if (has_side_effects((*(long long*)((uintptr_t)((n + 8)))))) {
      return 1;
    }
    arr = (*(long long*)((uintptr_t)((n + 16))));
    cnt = (*(long long*)((uintptr_t)((n + 24))));
    i = 0;
    while ((i < cnt)) {
      if (has_side_effects((*(long long*)((uintptr_t)((arr + (i * 8))))))) {
        return 1;
      }
      i = (i + 1);
    }
    return 0;
  }
  return 0;
  return 0;
}

long long classify_loop(long long wh) {
  long long w;
  long long body_arr, body_cnt, loop_var_p;
  long long i, pure_cnt, impure_cnt, dsp;
  w = wh;
  if (((*(long long*)((uintptr_t)((w + 32)))) > 0)) {
    if (((*(long long*)((uintptr_t)((w + 32)))) == 1)) {
      *(long long*)((uintptr_t)((w + 48))) = DSP_SEQ;
      return DSP_SEQ;
    }
    if (((*(long long*)((uintptr_t)((w + 32)))) == 2)) {
      *(long long*)((uintptr_t)((w + 48))) = DSP_PAR;
      return DSP_PAR;
    }
    if (((*(long long*)((uintptr_t)((w + 32)))) == 3)) {
      *(long long*)((uintptr_t)((w + 48))) = DSP_GPU;
      return DSP_GPU;
    }
  }
  body_arr = (*(long long*)((uintptr_t)((w + 16))));
  body_cnt = (*(long long*)((uintptr_t)((w + 24))));
  loop_var_p = find_loop_var(body_arr, body_cnt);
  find_strided(body_arr, body_cnt, loop_var_p);
  if ((loop_var_p != 0)) {
    dsp_add_local(loop_var_p);
  }
  i = 0;
  while ((i < arr_len(stride_vars))) {
    dsp_add_local(arr_get(stride_vars, i));
    i = (i + 1);
  }
  pure_cnt = 0;
  impure_cnt = 0;
  i = 0;
  while ((i < body_cnt)) {
    if (has_side_effects((*(long long*)((uintptr_t)((body_arr + (i * 8))))))) {
      impure_cnt = (impure_cnt + 1);
    } else {
      pure_cnt = (pure_cnt + 1);
    }
    i = (i + 1);
  }
  dsp = DSP_SEQ;
  if ((impure_cnt == 0)) {
    if ((arr_len(stride_vars) > 0)) {
      dsp = DSP_GPU;
    } else {
      dsp = DSP_PAR;
    }
  } else {
    if ((arr_len(stride_vars) > 0)) {
      if ((pure_cnt > 0)) {
        dsp = DSP_SPLIT;
      }
    }
  }
  *(long long*)((uintptr_t)((w + 48))) = dsp;
  return dsp;
  return 0;
}

uint8_t analyze_body(long long body_arr, long long body_cnt) {
  long long i;
  long long stmt;
  i = 0;
  while ((i < body_cnt)) {
    stmt = (*(long long*)((uintptr_t)((body_arr + (i * 8)))));
    if ((stmt != 0)) {
      if (((*(long long*)((uintptr_t)((stmt + 0)))) == T_WHILE)) {
        classify_loop(stmt);
        analyze_body((*(long long*)((uintptr_t)((stmt + 16)))), (*(long long*)((uintptr_t)((stmt + 24)))));
      }
      if (((*(long long*)((uintptr_t)((stmt + 0)))) == T_IF)) {
        analyze_body((*(long long*)((uintptr_t)((stmt + 16)))), (*(long long*)((uintptr_t)((stmt + 24)))));
        if (((*(long long*)((uintptr_t)((stmt + 40)))) > 0)) {
          analyze_body((*(long long*)((uintptr_t)((stmt + 32)))), (*(long long*)((uintptr_t)((stmt + 40)))));
        }
      }
    }
    i = (i + 1);
  }
  return 0;
  return 0;
}

uint8_t run_dispatch(void) {
  long long i, func, params_arr, param_cnt, body_arr, body_cnt, j;
  long long stmt, darr, dcnt, k;
  i = 0;
  while ((i < func_cnt)) {
    func = (*(long long*)((uintptr_t)((funcs + (i * 8)))));
    params_arr = (*(long long*)((uintptr_t)((func + FN_PARS))));
    param_cnt = (*(long long*)((uintptr_t)((func + FN_PCNT))));
    body_arr = (*(long long*)((uintptr_t)((func + FN_BODY))));
    body_cnt = (*(long long*)((uintptr_t)((func + FN_BCNT))));
    dsp_reset_locals();
    j = 0;
    while ((j < param_cnt)) {
      dsp_add_local((*(long long*)((uintptr_t)((params_arr + (j * 8))))));
      j = (j + 1);
    }
    j = 0;
    while ((j < body_cnt)) {
      stmt = (*(long long*)((uintptr_t)((body_arr + (j * 8)))));
      if ((stmt != 0)) {
        if (((*(long long*)((uintptr_t)((stmt + 0)))) == T_DECL)) {
          darr = (*(long long*)((uintptr_t)((stmt + 8))));
          dcnt = (*(long long*)((uintptr_t)((stmt + 16))));
          k = 0;
          while ((k < dcnt)) {
            dsp_add_local((*(long long*)((uintptr_t)((darr + (k * 8))))));
            k = (k + 1);
          }
        }
      }
      j = (j + 1);
    }
    analyze_body(body_arr, body_cnt);
    i = (i + 1);
  }
  return 0;
  return 0;
}

uint8_t val_is_builtin(long long name_p) {
  if (buf_eq((vbuf + unpack_s(name_p)), (long long)"malloc", unpack_l(name_p))) {
    if ((unpack_l(name_p) == 6)) {
      return 1;
    }
  }
  if (buf_eq((vbuf + unpack_s(name_p)), (long long)"free", unpack_l(name_p))) {
    if ((unpack_l(name_p) == 4)) {
      return 1;
    }
  }
  if (buf_eq((vbuf + unpack_s(name_p)), (long long)"realloc", unpack_l(name_p))) {
    if ((unpack_l(name_p) == 7)) {
      return 1;
    }
  }
  if (buf_eq((vbuf + unpack_s(name_p)), (long long)"memcpy", unpack_l(name_p))) {
    if ((unpack_l(name_p) == 6)) {
      return 1;
    }
  }
  if (buf_eq((vbuf + unpack_s(name_p)), (long long)"float_ret2", unpack_l(name_p))) {
    if ((unpack_l(name_p) == 10)) {
      return 1;
    }
  }
  if (buf_eq((vbuf + unpack_s(name_p)), (long long)"peek", unpack_l(name_p))) {
    if ((unpack_l(name_p) == 4)) {
      return 1;
    }
  }
  if (buf_eq((vbuf + unpack_s(name_p)), (long long)"poke", unpack_l(name_p))) {
    if ((unpack_l(name_p) == 4)) {
      return 1;
    }
  }
  if (buf_eq((vbuf + unpack_s(name_p)), (long long)"putchar", unpack_l(name_p))) {
    if ((unpack_l(name_p) == 7)) {
      return 1;
    }
  }
  if (buf_eq((vbuf + unpack_s(name_p)), (long long)"getchar", unpack_l(name_p))) {
    if ((unpack_l(name_p) == 7)) {
      return 1;
    }
  }
  if (buf_eq((vbuf + unpack_s(name_p)), (long long)"str_peek", unpack_l(name_p))) {
    if ((unpack_l(name_p) == 8)) {
      return 1;
    }
  }
  if (buf_eq((vbuf + unpack_s(name_p)), (long long)"argc_get", unpack_l(name_p))) {
    if ((unpack_l(name_p) == 8)) {
      return 1;
    }
  }
  if (buf_eq((vbuf + unpack_s(name_p)), (long long)"argv_get", unpack_l(name_p))) {
    if ((unpack_l(name_p) == 8)) {
      return 1;
    }
  }
  if (buf_eq((vbuf + unpack_s(name_p)), (long long)"fn_ptr", unpack_l(name_p))) {
    if ((unpack_l(name_p) == 6)) {
      return 1;
    }
  }
  if (buf_eq((vbuf + unpack_s(name_p)), (long long)"call", unpack_l(name_p))) {
    if ((unpack_l(name_p) == 4)) {
      return 1;
    }
  }
  if (buf_eq((vbuf + unpack_s(name_p)), (long long)"sys_write", unpack_l(name_p))) {
    if ((unpack_l(name_p) == 9)) {
      return 1;
    }
  }
  if (buf_eq((vbuf + unpack_s(name_p)), (long long)"sys_read", unpack_l(name_p))) {
    if ((unpack_l(name_p) == 8)) {
      return 1;
    }
  }
  if (buf_eq((vbuf + unpack_s(name_p)), (long long)"sys_open", unpack_l(name_p))) {
    if ((unpack_l(name_p) == 8)) {
      return 1;
    }
  }
  if (buf_eq((vbuf + unpack_s(name_p)), (long long)"sys_close", unpack_l(name_p))) {
    if ((unpack_l(name_p) == 9)) {
      return 1;
    }
  }
  if (buf_eq((vbuf + unpack_s(name_p)), (long long)"sys_fork", unpack_l(name_p))) {
    if ((unpack_l(name_p) == 8)) {
      return 1;
    }
  }
  if (buf_eq((vbuf + unpack_s(name_p)), (long long)"sys_execve", unpack_l(name_p))) {
    if ((unpack_l(name_p) == 10)) {
      return 1;
    }
  }
  if (buf_eq((vbuf + unpack_s(name_p)), (long long)"sys_waitpid", unpack_l(name_p))) {
    if ((unpack_l(name_p) == 11)) {
      return 1;
    }
  }
  if (buf_eq((vbuf + unpack_s(name_p)), (long long)"sys_exit", unpack_l(name_p))) {
    if ((unpack_l(name_p) == 8)) {
      return 1;
    }
  }
  if (buf_eq((vbuf + unpack_s(name_p)), (long long)"sys_ioctl", unpack_l(name_p))) {
    if ((unpack_l(name_p) == 9)) {
      return 1;
    }
  }
  if (buf_eq((vbuf + unpack_s(name_p)), (long long)"sys_nanosleep", unpack_l(name_p))) {
    if ((unpack_l(name_p) == 13)) {
      return 1;
    }
  }
  if (buf_eq((vbuf + unpack_s(name_p)), (long long)"sys_clock_gettime", unpack_l(name_p))) {
    if ((unpack_l(name_p) == 17)) {
      return 1;
    }
  }
  return 0;
  return 0;
}

uint8_t val_find_func(long long name_p) {
  long long i, rec;
  i = 0;
  while ((i < func_cnt)) {
    rec = (*(long long*)((uintptr_t)((funcs + (i * 8)))));
    if (packed_eq((*(long long*)((uintptr_t)((rec + FN_NAME)))), name_p)) {
      return i;
    }
    i = (i + 1);
  }
  return (-1);
  return 0;
}

uint8_t val_find_extern(long long name_p) {
  long long i, rec;
  i = 0;
  while ((i < ext_cnt)) {
    rec = (*(long long*)((uintptr_t)((externs + (i * 8)))));
    if (packed_eq((*(long long*)((uintptr_t)((rec + EX_NAME)))), name_p)) {
      return i;
    }
    i = (i + 1);
  }
  return (-1);
  return 0;
}

uint8_t val_check_node(long long nd) {
  long long n, t, name_p;
  if ((nd == 0)) {
    return 0;
  }
  n = nd;
  t = (*(long long*)((uintptr_t)((n + 0))));
  if ((t == T_CALL)) {
    long long fi, callee, expected, ai, arg_nd, arg_ty, par_ty;
    long long an;
    name_p = (*(long long*)((uintptr_t)((n + 8))));
    if ((val_is_builtin(name_p) == 0)) {
      if ((val_find_func(name_p) < 0)) {
        if ((val_find_extern(name_p) < 0)) {
          diag_fatal(201);
          diag_str((long long)"function '");
          diag_packed(name_p);
          diag_str((long long)"' called but never defined -- missing import?");
          diag_newline();
          bpp_sys_exit(1);
        }
      }
    }
    fi = val_find_func(name_p);
    if ((fi >= 0)) {
      callee = (*(long long*)((uintptr_t)((funcs + (fi * 8)))));
      expected = (*(long long*)((uintptr_t)((callee + 24))));
      if (((*(long long*)((uintptr_t)((n + 24)))) != expected)) {
        diag_warn(3);
        diag_str((long long)"'");
        diag_packed(name_p);
        diag_str((long long)"' expects ");
        diag_int(expected);
        diag_str((long long)" argument(s), got ");
        diag_int((*(long long*)((uintptr_t)((n + 24)))));
        diag_newline();
      }
      ai = 0;
      while ((ai < (*(long long*)((uintptr_t)((n + 24)))))) {
        arg_nd = (*(long long*)((uintptr_t)(((*(long long*)((uintptr_t)((n + 16)))) + (ai * 8)))));
        if ((arg_nd != 0)) {
          an = arg_nd;
          arg_ty = (*(long long*)((uintptr_t)((an + 56))));
          par_ty = get_fn_par_type(fi, ai);
          if ((arg_ty == TY_FLOAT)) {
            if ((par_ty != TY_FLOAT)) {
              diag_warn(2);
              diag_str((long long)"implicit float-to-int conversion in argument ");
              diag_int((ai + 1));
              diag_str((long long)" of '");
              diag_packed(name_p);
              diag_str((long long)"'");
              diag_newline();
              diag_help((long long)"add ': float' to the parameter declaration");
            }
          }
        }
        ai = (ai + 1);
      }
    }
    val_check_list((*(long long*)((uintptr_t)((n + 16)))), (*(long long*)((uintptr_t)((n + 24)))));
    return 0;
  }
  if ((t == T_ASSIGN)) {
    val_check_node((*(long long*)((uintptr_t)((n + 8)))));
    val_check_node((*(long long*)((uintptr_t)((n + 16)))));
    return 0;
  }
  if ((t == T_BINOP)) {
    val_check_node((*(long long*)((uintptr_t)((n + 16)))));
    val_check_node((*(long long*)((uintptr_t)((n + 24)))));
    return 0;
  }
  if ((t == T_UNARY)) {
    val_check_node((*(long long*)((uintptr_t)((n + 16)))));
    return 0;
  }
  if ((t == T_RET)) {
    val_check_node((*(long long*)((uintptr_t)((n + 8)))));
    return 0;
  }
  if ((t == T_IF)) {
    val_check_node((*(long long*)((uintptr_t)((n + 8)))));
    val_check_list((*(long long*)((uintptr_t)((n + 16)))), (*(long long*)((uintptr_t)((n + 24)))));
    if (((*(long long*)((uintptr_t)((n + 40)))) > 0)) {
      val_check_list((*(long long*)((uintptr_t)((n + 32)))), (*(long long*)((uintptr_t)((n + 40)))));
    }
    return 0;
  }
  if ((t == T_WHILE)) {
    val_check_node((*(long long*)((uintptr_t)((n + 8)))));
    val_check_list((*(long long*)((uintptr_t)((n + 16)))), (*(long long*)((uintptr_t)((n + 24)))));
    return 0;
  }
  if ((t == T_MEMLD)) {
    val_check_node((*(long long*)((uintptr_t)((n + 8)))));
    return 0;
  }
  if ((t == T_MEMST)) {
    val_check_node((*(long long*)((uintptr_t)((n + 8)))));
    val_check_node((*(long long*)((uintptr_t)((n + 16)))));
    return 0;
  }
  return 0;
  return 0;
}

uint8_t val_check_list(long long arr, long long cnt) {
  long long i, saw_ret, nd;
  long long sn;
  saw_ret = 0;
  i = 0;
  while ((i < cnt)) {
    nd = (*(long long*)((uintptr_t)((arr + (i * 8)))));
    if ((nd != 0)) {
      if (saw_ret) {
        diag_warn(5);
        diag_str((long long)"unreachable code after return");
        diag_newline();
        saw_ret = 0;
      }
      sn = nd;
      if (((*(long long*)((uintptr_t)((sn + 0)))) == T_RET)) {
        saw_ret = 1;
      }
    }
    val_check_node(nd);
    i = (i + 1);
  }
  return 0;
  return 0;
}

uint8_t run_validate(void) {
  long long i, rec, body_arr, body_cnt;
  i = 0;
  while ((i < func_cnt)) {
    rec = (*(long long*)((uintptr_t)((funcs + (i * 8)))));
    body_arr = (*(long long*)((uintptr_t)((rec + FN_BODY))));
    body_cnt = (*(long long*)((uintptr_t)((rec + FN_BCNT))));
    val_check_list(body_arr, body_cnt);
    i = (i + 1);
  }
  return 0;
  return 0;
}

uint8_t init_emitter(void) {
  ex_name = (long long)malloc(2048);
  ex_lib = (long long)malloc(2048);
  ex_ret = (long long)malloc(2048);
  ex_args = (long long)malloc(2048);
  ex_acnt = (long long)malloc(2048);
  ex_cnt = 0;
  fn_name = (long long)malloc(65536);
  fn_par = (long long)malloc(65536);
  fn_pcnt = (long long)malloc(65536);
  fn_body = (long long)malloc(65536);
  fn_bcnt = (long long)malloc(65536);
  fn_fidx = (long long)malloc(65536);
  fn_cnt = 0;
  gl_name = (long long)malloc(65536);
  gl_cnt = 0;
  ux_name = (long long)malloc(2048);
  ux_cnt = 0;
  g_indent = 0;
  return 0;
  return 0;
}

uint8_t bridge_data(void) {
  long long i, rec, name_p, idx, j;
  sbuf = vbuf;
  ex_cnt = ext_cnt;
  i = 0;
  while ((i < ext_cnt)) {
    rec = (*(long long*)((uintptr_t)((externs + (i * 8)))));
    *(long long*)((uintptr_t)((ex_name + (i * 8)))) = (*(long long*)((uintptr_t)((rec + EX_NAME))));
    *(long long*)((uintptr_t)((ex_lib + (i * 8)))) = (*(long long*)((uintptr_t)((rec + EX_LIB))));
    *(long long*)((uintptr_t)((ex_ret + (i * 8)))) = (*(long long*)((uintptr_t)((rec + EX_RET))));
    *(long long*)((uintptr_t)((ex_args + (i * 8)))) = (*(long long*)((uintptr_t)((rec + EX_ARGS))));
    *(long long*)((uintptr_t)((ex_acnt + (i * 8)))) = (*(long long*)((uintptr_t)((rec + EX_ACNT))));
    i = (i + 1);
  }
  fn_cnt = 0;
  i = 0;
  while ((i < func_cnt)) {
    rec = (*(long long*)((uintptr_t)((funcs + (i * 8)))));
    name_p = (*(long long*)((uintptr_t)((rec + FN_NAME))));
    idx = (-1);
    j = 0;
    while ((j < fn_cnt)) {
      if (str_eq_packed(name_p, (*(long long*)((uintptr_t)((fn_name + (j * 8))))))) {
        idx = j;
      }
      j = (j + 1);
    }
    if ((idx < 0)) {
      idx = fn_cnt;
      fn_cnt = (fn_cnt + 1);
    }
    *(long long*)((uintptr_t)((fn_name + (idx * 8)))) = name_p;
    *(long long*)((uintptr_t)((fn_par + (idx * 8)))) = (*(long long*)((uintptr_t)((rec + FN_PARS))));
    *(long long*)((uintptr_t)((fn_pcnt + (idx * 8)))) = (*(long long*)((uintptr_t)((rec + FN_PCNT))));
    *(long long*)((uintptr_t)((fn_body + (idx * 8)))) = (*(long long*)((uintptr_t)((rec + FN_BODY))));
    *(long long*)((uintptr_t)((fn_bcnt + (idx * 8)))) = (*(long long*)((uintptr_t)((rec + FN_BCNT))));
    *(long long*)((uintptr_t)((fn_fidx + (idx * 8)))) = i;
    i = (i + 1);
  }
  gl_cnt = glob_cnt;
  i = 0;
  while ((i < glob_cnt)) {
    *(long long*)((uintptr_t)((gl_name + (i * 8)))) = (*(long long*)((uintptr_t)((globals + (i * 8)))));
    i = (i + 1);
  }
  return 0;
  return 0;
}

uint8_t str_eq(long long p, long long lit, long long litlen) {
  if ((unpack_l(p) != litlen)) {
    return 0;
  }
  return buf_eq((sbuf + unpack_s(p)), lit, litlen);
  return 0;
}

uint8_t str_eq_packed(long long a, long long b) {
  long long as, al, bs, bl, i;
  al = unpack_l(a);
  bl = unpack_l(b);
  if ((al != bl)) {
    return 0;
  }
  as = unpack_s(a);
  bs = unpack_s(b);
  i = 0;
  while ((i < al)) {
    if ((((long long)(*(uint8_t*)((uintptr_t)(((sbuf + as) + i))))) != ((long long)(*(uint8_t*)((uintptr_t)(((sbuf + bs) + i))))))) {
      return 0;
    }
    i = (i + 1);
  }
  return 1;
  return 0;
}

uint8_t print_p(long long p) {
  print_buf((sbuf + unpack_s(p)), unpack_l(p));
  return 0;
  return 0;
}

uint8_t is_extern(long long name_p) {
  long long i;
  i = 0;
  while ((i < ex_cnt)) {
    if (str_eq_packed(name_p, (*(long long*)((uintptr_t)((ex_name + (i * 8))))))) {
      return 1;
    }
    i = (i + 1);
  }
  return 0;
  return 0;
}

uint8_t mark_used(long long name_p) {
  long long i;
  i = 0;
  while ((i < ux_cnt)) {
    if (str_eq_packed(name_p, (*(long long*)((uintptr_t)((ux_name + (i * 8))))))) {
      return 0;
    }
    i = (i + 1);
  }
  *(long long*)((uintptr_t)((ux_name + (ux_cnt * 8)))) = name_p;
  ux_cnt = (ux_cnt + 1);
  return 0;
  return 0;
}

uint8_t is_used(long long name_p) {
  long long i;
  i = 0;
  while ((i < ux_cnt)) {
    if (str_eq_packed(name_p, (*(long long*)((uintptr_t)((ux_name + (i * 8))))))) {
      return 1;
    }
    i = (i + 1);
  }
  return 0;
  return 0;
}

uint8_t scan_calls(long long nd) {
  long long n;
  long long t, i, arr, cnt;
  if ((nd == 0)) {
    return 0;
  }
  n = nd;
  t = (*(long long*)((uintptr_t)((n + 0))));
  if ((t == T_CALL)) {
    if (is_extern((*(long long*)((uintptr_t)((n + 8)))))) {
      mark_used((*(long long*)((uintptr_t)((n + 8)))));
    }
    arr = (*(long long*)((uintptr_t)((n + 16))));
    cnt = (*(long long*)((uintptr_t)((n + 24))));
    i = 0;
    while ((i < cnt)) {
      scan_calls((*(long long*)((uintptr_t)((arr + (i * 8))))));
      i = (i + 1);
    }
    return 0;
  }
  if ((t == T_BINOP)) {
    scan_calls((*(long long*)((uintptr_t)((n + 16)))));
    scan_calls((*(long long*)((uintptr_t)((n + 24)))));
    return 0;
  }
  if ((t == T_UNARY)) {
    return scan_calls((*(long long*)((uintptr_t)((n + 16)))));
  }
  if ((t == T_ASSIGN)) {
    scan_calls((*(long long*)((uintptr_t)((n + 8)))));
    scan_calls((*(long long*)((uintptr_t)((n + 16)))));
    return 0;
  }
  if ((t == T_MEMLD)) {
    return scan_calls((*(long long*)((uintptr_t)((n + 8)))));
  }
  if ((t == T_MEMST)) {
    scan_calls((*(long long*)((uintptr_t)((n + 8)))));
    scan_calls((*(long long*)((uintptr_t)((n + 16)))));
    return 0;
  }
  if ((t == T_RET)) {
    return scan_calls((*(long long*)((uintptr_t)((n + 8)))));
  }
  if ((t == T_IF)) {
    scan_calls((*(long long*)((uintptr_t)((n + 8)))));
    arr = (*(long long*)((uintptr_t)((n + 16))));
    cnt = (*(long long*)((uintptr_t)((n + 24))));
    i = 0;
    while ((i < cnt)) {
      scan_calls((*(long long*)((uintptr_t)((arr + (i * 8))))));
      i = (i + 1);
    }
    if (((*(long long*)((uintptr_t)((n + 40)))) > 0)) {
      arr = (*(long long*)((uintptr_t)((n + 32))));
      cnt = (*(long long*)((uintptr_t)((n + 40))));
      i = 0;
      while ((i < cnt)) {
        scan_calls((*(long long*)((uintptr_t)((arr + (i * 8))))));
        i = (i + 1);
      }
    }
    return 0;
  }
  if ((t == T_WHILE)) {
    scan_calls((*(long long*)((uintptr_t)((n + 8)))));
    arr = (*(long long*)((uintptr_t)((n + 16))));
    cnt = (*(long long*)((uintptr_t)((n + 24))));
    i = 0;
    while ((i < cnt)) {
      scan_calls((*(long long*)((uintptr_t)((arr + (i * 8))))));
      i = (i + 1);
    }
    return 0;
  }
  return 0;
  return 0;
}

uint8_t find_used_externs(void) {
  long long fi, bi, arr, cnt;
  fi = 0;
  while ((fi < fn_cnt)) {
    arr = (*(long long*)((uintptr_t)((fn_body + (fi * 8)))));
    cnt = (*(long long*)((uintptr_t)((fn_bcnt + (fi * 8)))));
    bi = 0;
    while ((bi < cnt)) {
      scan_calls((*(long long*)((uintptr_t)((arr + (bi * 8))))));
      bi = (bi + 1);
    }
    fi = (fi + 1);
  }
  return 0;
  return 0;
}

uint8_t find_extern(long long name_p) {
  long long i;
  i = 0;
  while ((i < ex_cnt)) {
    if (str_eq_packed(name_p, (*(long long*)((uintptr_t)((ex_name + (i * 8))))))) {
      return i;
    }
    i = (i + 1);
  }
  return (-1);
  return 0;
}

uint8_t is_ptr_type(long long tp) {
  if (str_eq(tp, (long long)"char*", 5)) {
    return 1;
  }
  if (str_eq(tp, (long long)"void*", 5)) {
    return 1;
  }
  if (str_eq(tp, (long long)"ptr", 3)) {
    return 1;
  }
  if (str_eq(tp, (long long)"word", 4)) {
    return 1;
  }
  return 0;
  return 0;
}

uint8_t emit_val_cast(long long tp) {
  if (str_eq(tp, (long long)"int", 3)) {
    out((long long)"(int)");
    return 1;
  }
  if (str_eq(tp, (long long)"char", 4)) {
    out((long long)"(char)");
    return 1;
  }
  if (str_eq(tp, (long long)"byte", 4)) {
    out((long long)"(uint8_t)");
    return 1;
  }
  if (str_eq(tp, (long long)"half", 4)) {
    out((long long)"(uint16_t)");
    return 1;
  }
  if (str_eq(tp, (long long)"long", 4)) {
    out((long long)"(long long)");
    return 1;
  }
  return 0;
  return 0;
}

uint8_t emit_ctype(long long tp) {
  if (str_eq(tp, (long long)"int", 3)) {
    out((long long)"int");
    return 0;
  }
  if (str_eq(tp, (long long)"void", 4)) {
    out((long long)"void");
    return 0;
  }
  if (str_eq(tp, (long long)"char*", 5)) {
    out((long long)"const char*");
    return 0;
  }
  if (str_eq(tp, (long long)"void*", 5)) {
    out((long long)"void*");
    return 0;
  }
  if (str_eq(tp, (long long)"ptr", 3)) {
    out((long long)"void*");
    return 0;
  }
  if (str_eq(tp, (long long)"word", 4)) {
    out((long long)"unsigned int");
    return 0;
  }
  if (str_eq(tp, (long long)"long", 4)) {
    out((long long)"long long");
    return 0;
  }
  print_p(tp);
  return 0;
  return 0;
}

uint8_t emit_node(long long nd) {
  long long n, an;
  long long t, i, arr, cnt, name_p, exi;
  long long atype_arr, atype_cnt, atp, arg_node, ret_tp;
  long long is_str_lit, vs, op_p;
  if ((nd == 0)) {
    out((long long)"0");
    return 0;
  }
  n = nd;
  t = (*(long long*)((uintptr_t)((n + 0))));
  if ((t == T_LIT)) {
    if ((((long long)(*(uint8_t*)((uintptr_t)((sbuf + unpack_s((*(long long*)((uintptr_t)((n + 8)))))))))) == 34)) {
      out((long long)"(long long)");
    }
    print_p((*(long long*)((uintptr_t)((n + 8)))));
    return 0;
  }
  if ((t == T_VAR)) {
    if (is_var_stack((*(long long*)((uintptr_t)((n + 8)))))) {
      out((long long)"((long long)(");
      print_p((*(long long*)((uintptr_t)((n + 8)))));
      out((long long)"))");
      return 0;
    }
    print_p((*(long long*)((uintptr_t)((n + 8)))));
    return 0;
  }
  if ((t == T_NOP)) {
    return 0;
  }
  if ((t == T_BINOP)) {
    out((long long)"(");
    emit_node((*(long long*)((uintptr_t)((n + 16)))));
    out((long long)" ");
    print_p((*(long long*)((uintptr_t)((n + 8)))));
    out((long long)" ");
    emit_node((*(long long*)((uintptr_t)((n + 24)))));
    out((long long)")");
    return 0;
  }
  if ((t == T_UNARY)) {
    out((long long)"(");
    print_p((*(long long*)((uintptr_t)((n + 8)))));
    emit_node((*(long long*)((uintptr_t)((n + 16)))));
    out((long long)")");
    return 0;
  }
  if ((t == T_ASSIGN)) {
    an = (*(long long*)((uintptr_t)((n + 8))));
    if (((*(long long*)((uintptr_t)((an + 0)))) == T_VAR)) {
      if (is_var_stack((*(long long*)((uintptr_t)((an + 8)))))) {
        long long _sidx, _fc;
        _sidx = get_var_type((*(long long*)((uintptr_t)((an + 8)))));
        _fc = (get_struct_size(_sidx) / 8);
        out((long long)"memcpy(");
        print_p((*(long long*)((uintptr_t)((an + 8)))));
        out((long long)", ");
        an = (*(long long*)((uintptr_t)((n + 16))));
        if (((*(long long*)((uintptr_t)((an + 0)))) == T_VAR)) {
          print_p((*(long long*)((uintptr_t)((an + 8)))));
        } else {
          emit_node((*(long long*)((uintptr_t)((n + 16)))));
        }
        out((long long)", ");
        emit_int((_fc * 8));
        out((long long)")");
        return 0;
      }
    }
    emit_node((*(long long*)((uintptr_t)((n + 8)))));
    out((long long)" = ");
    emit_node((*(long long*)((uintptr_t)((n + 16)))));
    return 0;
  }
  if ((t == T_MEMLD)) {
    out((long long)"(*(long long*)((uintptr_t)(");
    emit_node((*(long long*)((uintptr_t)((n + 8)))));
    out((long long)")))");
    return 0;
  }
  if ((t == T_MEMST)) {
    out((long long)"*(long long*)((uintptr_t)(");
    emit_node((*(long long*)((uintptr_t)((n + 8)))));
    out((long long)")) = ");
    emit_node((*(long long*)((uintptr_t)((n + 16)))));
    return 0;
  }
  if ((t == T_RET)) {
    out((long long)"return ");
    emit_node((*(long long*)((uintptr_t)((n + 8)))));
    return 0;
  }
  if ((t == T_CALL)) {
    name_p = (*(long long*)((uintptr_t)((n + 8))));
    arr = (*(long long*)((uintptr_t)((n + 16))));
    cnt = (*(long long*)((uintptr_t)((n + 24))));
    if (str_eq(name_p, (long long)"malloc", 6)) {
      out((long long)"(long long)malloc(");
      if ((cnt > 0)) {
        emit_node((*(long long*)((uintptr_t)((arr + 0)))));
      }
      out((long long)")");
      return 0;
    }
    if (str_eq(name_p, (long long)"free", 4)) {
      out((long long)"(free((void*)(uintptr_t)(");
      if ((cnt > 0)) {
        emit_node((*(long long*)((uintptr_t)((arr + 0)))));
      }
      out((long long)")), 0)");
      return 0;
    }
    if (str_eq(name_p, (long long)"realloc", 7)) {
      out((long long)"(long long)(uintptr_t)realloc((void*)(uintptr_t)(");
      if ((cnt > 0)) {
        emit_node((*(long long*)((uintptr_t)((arr + 0)))));
      }
      out((long long)"), (size_t)(");
      if ((cnt > 2)) {
        emit_node((*(long long*)((uintptr_t)((arr + 16)))));
      } else {
        if ((cnt > 1)) {
          emit_node((*(long long*)((uintptr_t)((arr + 8)))));
        }
      }
      out((long long)"))");
      return 0;
    }
    if (str_eq(name_p, (long long)"memcpy", 6)) {
      out((long long)"(long long)(uintptr_t)memcpy((void*)(uintptr_t)(");
      if ((cnt > 0)) {
        emit_node((*(long long*)((uintptr_t)((arr + 0)))));
      }
      out((long long)"), (const void*)(uintptr_t)(");
      if ((cnt > 1)) {
        emit_node((*(long long*)((uintptr_t)((arr + 8)))));
      }
      out((long long)"), (size_t)(");
      if ((cnt > 2)) {
        emit_node((*(long long*)((uintptr_t)((arr + 16)))));
      }
      out((long long)"))");
      return 0;
    }
    if (str_eq(name_p, (long long)"peek", 4)) {
      out((long long)"((long long)(*(uint8_t*)((uintptr_t)(");
      if ((cnt > 0)) {
        emit_node((*(long long*)((uintptr_t)((arr + 0)))));
      }
      out((long long)"))))");
      return 0;
    }
    if (str_eq(name_p, (long long)"poke", 4)) {
      out((long long)"(*(uint8_t*)((uintptr_t)(");
      if ((cnt > 0)) {
        emit_node((*(long long*)((uintptr_t)((arr + 0)))));
      }
      out((long long)")) = (uint8_t)(");
      if ((cnt > 1)) {
        emit_node((*(long long*)((uintptr_t)((arr + 8)))));
      }
      out((long long)"))");
      return 0;
    }
    if (str_eq(name_p, (long long)"putchar", 7)) {
      out((long long)"(_bpp_scratch[0] = (uint8_t)(");
      if ((cnt > 0)) {
        emit_node((*(long long*)((uintptr_t)((arr + 0)))));
      }
      out((long long)"), bpp_sys_write(1, _bpp_scratch, 1))");
      return 0;
    }
    if (str_eq(name_p, (long long)"getchar", 7)) {
      out((long long)"(_bpp_scratch[0] = 0, bpp_sys_read(0, _bpp_scratch, 1) > 0 ? (long long)_bpp_scratch[0] : -1LL)");
      return 0;
    }
    if (str_eq(name_p, (long long)"str_peek", 8)) {
      out((long long)"(long long)((uint8_t*)((uintptr_t)(");
      if ((cnt > 0)) {
        emit_node((*(long long*)((uintptr_t)((arr + 0)))));
      }
      out((long long)")))[(");
      if ((cnt > 1)) {
        emit_node((*(long long*)((uintptr_t)((arr + 8)))));
      }
      out((long long)")]");
      return 0;
    }
    if (str_eq(name_p, (long long)"sys_open", 8)) {
      out((long long)"bpp_sys_open((const char*)((uintptr_t)(");
      if ((cnt > 0)) {
        emit_node((*(long long*)((uintptr_t)((arr + 0)))));
      }
      out((long long)")), ");
      if ((cnt > 1)) {
        emit_node((*(long long*)((uintptr_t)((arr + 8)))));
      }
      out((long long)")");
      return 0;
    }
    if (str_eq(name_p, (long long)"sys_read", 8)) {
      out((long long)"bpp_sys_read(");
      if ((cnt > 0)) {
        emit_node((*(long long*)((uintptr_t)((arr + 0)))));
      }
      out((long long)", (void*)((uintptr_t)(");
      if ((cnt > 1)) {
        emit_node((*(long long*)((uintptr_t)((arr + 8)))));
      }
      out((long long)")), ");
      if ((cnt > 2)) {
        emit_node((*(long long*)((uintptr_t)((arr + 16)))));
      }
      out((long long)")");
      return 0;
    }
    if (str_eq(name_p, (long long)"sys_close", 9)) {
      out((long long)"bpp_sys_close(");
      if ((cnt > 0)) {
        emit_node((*(long long*)((uintptr_t)((arr + 0)))));
      }
      out((long long)")");
      return 0;
    }
    if (str_eq(name_p, (long long)"sys_fork", 8)) {
      out((long long)"bpp_sys_fork()");
      return 0;
    }
    if (str_eq(name_p, (long long)"sys_execve", 10)) {
      out((long long)"bpp_sys_execve(");
      emit_node((*(long long*)((uintptr_t)((arr + 0)))));
      out((long long)", ");
      emit_node((*(long long*)((uintptr_t)((arr + 8)))));
      out((long long)", ");
      emit_node((*(long long*)((uintptr_t)((arr + 16)))));
      out((long long)")");
      return 0;
    }
    if (str_eq(name_p, (long long)"sys_waitpid", 11)) {
      out((long long)"bpp_sys_waitpid(");
      emit_node((*(long long*)((uintptr_t)((arr + 0)))));
      out((long long)")");
      return 0;
    }
    if (str_eq(name_p, (long long)"sys_exit", 8)) {
      out((long long)"bpp_sys_exit(");
      emit_node((*(long long*)((uintptr_t)((arr + 0)))));
      out((long long)")");
      return 0;
    }
    if (str_eq(name_p, (long long)"sys_write", 9)) {
      out((long long)"bpp_sys_write(");
      if ((cnt > 0)) {
        emit_node((*(long long*)((uintptr_t)((arr + 0)))));
      }
      out((long long)", (void*)((uintptr_t)(");
      if ((cnt > 1)) {
        emit_node((*(long long*)((uintptr_t)((arr + 8)))));
      }
      out((long long)")), ");
      if ((cnt > 2)) {
        emit_node((*(long long*)((uintptr_t)((arr + 16)))));
      }
      out((long long)")");
      return 0;
    }
    if (str_eq(name_p, (long long)"argv_get", 8)) {
      out((long long)"((long long)_bpp_argv[(int)(");
      if ((cnt > 0)) {
        emit_node((*(long long*)((uintptr_t)((arr + 0)))));
      }
      out((long long)")])");
      return 0;
    }
    if (str_eq(name_p, (long long)"argc_get", 8)) {
      out((long long)"((long long)_bpp_argc)");
      return 0;
    }
    if (str_eq(name_p, (long long)"fn_ptr", 6)) {
      out((long long)"((long long)(uintptr_t)");
      emit_node((*(long long*)((uintptr_t)((arr + 0)))));
      out((long long)")");
      return 0;
    }
    if (str_eq(name_p, (long long)"call", 4)) {
      out((long long)"((long long(*)(");
      i = 1;
      while ((i < cnt)) {
        if ((i > 1)) {
          out((long long)",");
        }
        out((long long)"long long");
        i = (i + 1);
      }
      out((long long)"))((uintptr_t)(");
      emit_node((*(long long*)((uintptr_t)((arr + 0)))));
      out((long long)")))(");
      i = 1;
      while ((i < cnt)) {
        if ((i > 1)) {
          out((long long)", ");
        }
        emit_node((*(long long*)((uintptr_t)((arr + (i * 8))))));
        i = (i + 1);
      }
      out((long long)")");
      return 0;
    }
    exi = find_extern(name_p);
    if ((exi >= 0)) {
      ret_tp = (*(long long*)((uintptr_t)((ex_ret + (exi * 8)))));
      if (is_ptr_type(ret_tp)) {
        out((long long)"(long long)");
      }
      print_p(name_p);
      out((long long)"(");
      atype_arr = (*(long long*)((uintptr_t)((ex_args + (exi * 8)))));
      atype_cnt = (*(long long*)((uintptr_t)((ex_acnt + (exi * 8)))));
      i = 0;
      while ((i < cnt)) {
        if ((i > 0)) {
          out((long long)", ");
        }
        if ((i < atype_cnt)) {
          atp = (*(long long*)((uintptr_t)((atype_arr + (i * 8)))));
          arg_node = (*(long long*)((uintptr_t)((arr + (i * 8)))));
          if (is_ptr_type(atp)) {
            out((long long)"(const char*)((uintptr_t)(");
            emit_node(arg_node);
            out((long long)"))");
          } else {
            emit_val_cast(atp);
            emit_node(arg_node);
          }
        } else {
          emit_node((*(long long*)((uintptr_t)((arr + (i * 8))))));
        }
        i = (i + 1);
      }
      out((long long)")");
      return 0;
    }
    emit_fname(name_p);
    out((long long)"(");
    i = 0;
    while ((i < cnt)) {
      if ((i > 0)) {
        out((long long)", ");
      }
      emit_node((*(long long*)((uintptr_t)((arr + (i * 8))))));
      i = (i + 1);
    }
    out((long long)")");
    return 0;
  }
  if ((t == T_IF)) {
    out((long long)"if (");
    emit_node((*(long long*)((uintptr_t)((n + 8)))));
    out((long long)") {");
    (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
    g_indent = (g_indent + 1);
    emit_body((*(long long*)((uintptr_t)((n + 16)))), (*(long long*)((uintptr_t)((n + 24)))));
    g_indent = (g_indent - 1);
    emit_pad();
    out((long long)"}");
    if (((*(long long*)((uintptr_t)((n + 40)))) > 0)) {
      out((long long)" else {");
      (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
      g_indent = (g_indent + 1);
      emit_body((*(long long*)((uintptr_t)((n + 32)))), (*(long long*)((uintptr_t)((n + 40)))));
      g_indent = (g_indent - 1);
      emit_pad();
      out((long long)"}");
    }
    return 0;
  }
  if ((t == T_WHILE)) {
    out((long long)"while (");
    emit_node((*(long long*)((uintptr_t)((n + 8)))));
    out((long long)") {");
    (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
    g_indent = (g_indent + 1);
    emit_body((*(long long*)((uintptr_t)((n + 16)))), (*(long long*)((uintptr_t)((n + 24)))));
    g_indent = (g_indent - 1);
    emit_pad();
    out((long long)"}");
    return 0;
  }
  if ((t == T_BREAK)) {
    out((long long)"break");
    return 0;
  }
  return 0;
  return 0;
}

uint8_t emit_body(long long arr, long long cnt) {
  long long i, di, darr, dcnt, has_int, has_flt, vty;
  long long n;
  i = 0;
  while ((i < cnt)) {
    n = (*(long long*)((uintptr_t)((arr + (i * 8)))));
    if ((n != 0)) {
      if (((*(long long*)((uintptr_t)((n + 0)))) == T_NOP)) {
      } else {
        if (((*(long long*)((uintptr_t)((n + 0)))) == T_DECL)) {
          darr = (*(long long*)((uintptr_t)((n + 8))));
          dcnt = (*(long long*)((uintptr_t)((n + 16))));
          if (((*(long long*)((uintptr_t)((n + 32)))) == 1)) {
            long long sidx, fc;
            sidx = get_var_type((*(long long*)((uintptr_t)((darr + 0)))));
            fc = (get_struct_size(sidx) / 8);
            emit_pad();
            out((long long)"long long ");
            print_p((*(long long*)((uintptr_t)((darr + 0)))));
            out((long long)"[");
            emit_int(fc);
            out((long long)"];");
            (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
          } else {
            long long dh;
            dh = (*(long long*)((uintptr_t)((n + 24))));
            if ((dh == TY_BYTE)) {
              di = 0;
              has_int = 0;
              while ((di < dcnt)) {
                vty = get_fn_var_type(cur_fn_idx, (*(long long*)((uintptr_t)((darr + (di * 8))))));
                if ((vty != TY_FLOAT)) {
                  if ((has_int == 0)) {
                    emit_pad();
                    out((long long)"uint8_t ");
                    has_int = 1;
                  } else {
                    out((long long)", ");
                  }
                  print_p((*(long long*)((uintptr_t)((darr + (di * 8))))));
                }
                di = (di + 1);
              }
              if (has_int) {
                out((long long)";");
                (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
              }
            } else {
              if ((dh == TY_QUART)) {
                di = 0;
                has_int = 0;
                while ((di < dcnt)) {
                  vty = get_fn_var_type(cur_fn_idx, (*(long long*)((uintptr_t)((darr + (di * 8))))));
                  if ((vty != TY_FLOAT)) {
                    if ((has_int == 0)) {
                      emit_pad();
                      out((long long)"uint16_t ");
                      has_int = 1;
                    } else {
                      out((long long)", ");
                    }
                    print_p((*(long long*)((uintptr_t)((darr + (di * 8))))));
                  }
                  di = (di + 1);
                }
                if (has_int) {
                  out((long long)";");
                  (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
                }
              } else {
                if ((dh == TY_HALF)) {
                  di = 0;
                  has_int = 0;
                  while ((di < dcnt)) {
                    vty = get_fn_var_type(cur_fn_idx, (*(long long*)((uintptr_t)((darr + (di * 8))))));
                    if ((vty != TY_FLOAT)) {
                      if ((has_int == 0)) {
                        emit_pad();
                        out((long long)"uint32_t ");
                        has_int = 1;
                      } else {
                        out((long long)", ");
                      }
                      print_p((*(long long*)((uintptr_t)((darr + (di * 8))))));
                    }
                    di = (di + 1);
                  }
                  if (has_int) {
                    out((long long)";");
                    (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
                  }
                } else {
                  has_int = 0;
                  di = 0;
                  while ((di < dcnt)) {
                    vty = get_fn_var_type(cur_fn_idx, (*(long long*)((uintptr_t)((darr + (di * 8))))));
                    if ((vty != TY_FLOAT)) {
                      if ((has_int == 0)) {
                        emit_pad();
                        out((long long)"long long ");
                        has_int = 1;
                      } else {
                        out((long long)", ");
                      }
                      print_p((*(long long*)((uintptr_t)((darr + (di * 8))))));
                    }
                    di = (di + 1);
                  }
                  if (has_int) {
                    out((long long)";");
                    (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
                  }
                }
              }
            }
            has_flt = 0;
            di = 0;
            while ((di < dcnt)) {
              vty = get_fn_var_type(cur_fn_idx, (*(long long*)((uintptr_t)((darr + (di * 8))))));
              if ((vty == TY_FLOAT)) {
                if ((has_flt == 0)) {
                  emit_pad();
                  out((long long)"double ");
                  has_flt = 1;
                } else {
                  out((long long)", ");
                }
                print_p((*(long long*)((uintptr_t)((darr + (di * 8))))));
              }
              di = (di + 1);
            }
            if (has_flt) {
              out((long long)";");
              (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
            }
          }
        } else {
          if ((((*(long long*)((uintptr_t)((n + 0)))) == T_IF) | ((*(long long*)((uintptr_t)((n + 0)))) == T_WHILE))) {
            emit_pad();
            emit_node(n);
            (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
          } else {
            emit_pad();
            emit_node(n);
            out((long long)";");
            (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
          }
        }
      }
    }
    i = (i + 1);
  }
  return 0;
  return 0;
}

uint8_t emit_pad(void) {
  long long i;
  i = 0;
  while ((i < g_indent)) {
    out((long long)"  ");
    i = (i + 1);
  }
  return 0;
  return 0;
}

uint8_t emit_int(long long n) {
  if ((n >= 10)) {
    emit_int((n / 10));
  }
  (_bpp_scratch[0] = (uint8_t)((48 + (n % 10))), bpp_sys_write(1, _bpp_scratch, 1));
  return 0;
  return 0;
}

uint8_t emit_fname(long long name_p) {
  if (str_eq(name_p, (long long)"main", 4)) {
    out((long long)"main_bpp");
  } else {
    print_p(name_p);
  }
  return 0;
  return 0;
}

uint8_t emit_includes(void) {
  long long i, j, args, acnt;
  out((long long)"#include <stdint.h>");
  (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
  out((long long)"#include <unistd.h>");
  (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
  out((long long)"#include <sys/wait.h>");
  (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
  (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
  i = 0;
  while ((i < ex_cnt)) {
    if (is_used((*(long long*)((uintptr_t)((ex_name + (i * 8))))))) {
      emit_ctype((*(long long*)((uintptr_t)((ex_ret + (i * 8))))));
      out((long long)" ");
      print_p((*(long long*)((uintptr_t)((ex_name + (i * 8))))));
      out((long long)"(");
      args = (*(long long*)((uintptr_t)((ex_args + (i * 8)))));
      acnt = (*(long long*)((uintptr_t)((ex_acnt + (i * 8)))));
      j = 0;
      while ((j < acnt)) {
        if ((j > 0)) {
          out((long long)", ");
        }
        emit_ctype((*(long long*)((uintptr_t)((args + (j * 8))))));
        j = (j + 1);
      }
      out((long long)");");
      (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
    }
    i = (i + 1);
  }
  (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
  return 0;
  return 0;
}

uint8_t emit_q(long long s) {
  (_bpp_scratch[0] = (uint8_t)(34), bpp_sys_write(1, _bpp_scratch, 1));
  out(s);
  (_bpp_scratch[0] = (uint8_t)(34), bpp_sys_write(1, _bpp_scratch, 1));
  return 0;
  return 0;
}

uint8_t emit_svc(void) {
  (_bpp_scratch[0] = (uint8_t)(34), bpp_sys_write(1, _bpp_scratch, 1));
  out((long long)"svc #0x80");
  (_bpp_scratch[0] = (uint8_t)(92), bpp_sys_write(1, _bpp_scratch, 1));
  (_bpp_scratch[0] = (uint8_t)(110), bpp_sys_write(1, _bpp_scratch, 1));
  (_bpp_scratch[0] = (uint8_t)(92), bpp_sys_write(1, _bpp_scratch, 1));
  (_bpp_scratch[0] = (uint8_t)(116), bpp_sys_write(1, _bpp_scratch, 1));
  out((long long)"csneg x0, x0, x0, cc");
  (_bpp_scratch[0] = (uint8_t)(34), bpp_sys_write(1, _bpp_scratch, 1));
  return 0;
  return 0;
}

uint8_t emit_reg(long long reg, long long val) {
  out((long long)"  register long long ");
  out(reg);
  out((long long)" __asm__(");
  emit_q(reg);
  out((long long)") = ");
  out(val);
  out((long long)";");
  (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
  return 0;
  return 0;
}

uint8_t emit_runtime(void) {
  out((long long)"// B++ I/O runtime — raw syscalls.");
  (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
  out((long long)"static long long bpp_sys_write(long long fd, const void* buf, long long len) {");
  (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
  emit_reg((long long)"x0", (long long)"fd");
  emit_reg((long long)"x1", (long long)"(long long)buf");
  emit_reg((long long)"x2", (long long)"len");
  emit_reg((long long)"x16", (long long)"4");
  out((long long)"  __asm__ volatile(");
  emit_svc();
  out((long long)" : ");
  emit_q((long long)"+r");
  out((long long)"(x0) : ");
  emit_q((long long)"r");
  out((long long)"(x1), ");
  emit_q((long long)"r");
  out((long long)"(x2), ");
  emit_q((long long)"r");
  out((long long)"(x16) : ");
  emit_q((long long)"memory");
  out((long long)", ");
  emit_q((long long)"cc");
  out((long long)");");
  (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
  out((long long)"  return x0;");
  (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
  out((long long)"}");
  (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
  out((long long)"static long long bpp_sys_read(long long fd, void* buf, long long len) {");
  (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
  emit_reg((long long)"x0", (long long)"fd");
  emit_reg((long long)"x1", (long long)"(long long)buf");
  emit_reg((long long)"x2", (long long)"len");
  emit_reg((long long)"x16", (long long)"3");
  out((long long)"  __asm__ volatile(");
  emit_svc();
  out((long long)" : ");
  emit_q((long long)"+r");
  out((long long)"(x0) : ");
  emit_q((long long)"r");
  out((long long)"(x1), ");
  emit_q((long long)"r");
  out((long long)"(x2), ");
  emit_q((long long)"r");
  out((long long)"(x16) : ");
  emit_q((long long)"memory");
  out((long long)", ");
  emit_q((long long)"cc");
  out((long long)");");
  (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
  out((long long)"  return x0;");
  (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
  out((long long)"}");
  (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
  out((long long)"static long long bpp_sys_open(const char* path, long long flags) {");
  (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
  emit_reg((long long)"x0", (long long)"(long long)path");
  emit_reg((long long)"x1", (long long)"flags");
  emit_reg((long long)"x2", (long long)"493");
  emit_reg((long long)"x16", (long long)"5");
  out((long long)"  __asm__ volatile(");
  emit_svc();
  out((long long)" : ");
  emit_q((long long)"+r");
  out((long long)"(x0) : ");
  emit_q((long long)"r");
  out((long long)"(x1), ");
  emit_q((long long)"r");
  out((long long)"(x2), ");
  emit_q((long long)"r");
  out((long long)"(x16) : ");
  emit_q((long long)"memory");
  out((long long)", ");
  emit_q((long long)"cc");
  out((long long)");");
  (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
  out((long long)"  return x0;");
  (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
  out((long long)"}");
  (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
  out((long long)"static long long bpp_sys_close(long long fd) {");
  (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
  emit_reg((long long)"x0", (long long)"fd");
  emit_reg((long long)"x16", (long long)"6");
  out((long long)"  __asm__ volatile(");
  emit_svc();
  out((long long)" : ");
  emit_q((long long)"+r");
  out((long long)"(x0) : ");
  emit_q((long long)"r");
  out((long long)"(x16) : ");
  emit_q((long long)"memory");
  out((long long)", ");
  emit_q((long long)"cc");
  out((long long)");");
  (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
  out((long long)"  return x0;");
  (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
  out((long long)"}");
  (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
  out((long long)"static long long bpp_sys_fork(void) { return (long long)fork(); }");
  (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
  out((long long)"static long long bpp_sys_execve(long long p, long long a, long long e) {");
  (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
  out((long long)"  return (long long)execve((const char*)(uintptr_t)p,(char*const*)(uintptr_t)a,(char*const*)(uintptr_t)e); }");
  (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
  out((long long)"static long long bpp_sys_waitpid(long long pid) { int s; waitpid((pid_t)pid,&s,0); return 0; }");
  (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
  out((long long)"static long long bpp_sys_exit(long long c) { _exit((int)c); return 0; }");
  (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
  (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
  return 0;
  return 0;
}

uint8_t emit_mem_model(void) {
  out((long long)"#include <stdlib.h>");
  (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
  out((long long)"#include <string.h>");
  (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
  out((long long)"static uint8_t _bpp_scratch[16];");
  (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
  out((long long)"static int _bpp_argc;");
  (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
  out((long long)"static char** _bpp_argv;");
  (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
  (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
  return 0;
  return 0;
}

uint8_t emit_globals(void) {
  long long i, j, dup;
  i = 0;
  while ((i < gl_cnt)) {
    dup = 0;
    j = 0;
    while ((j < i)) {
      if (str_eq_packed((*(long long*)((uintptr_t)((gl_name + (i * 8))))), (*(long long*)((uintptr_t)((gl_name + (j * 8))))))) {
        dup = 1;
      }
      j = (j + 1);
    }
    if ((dup == 0)) {
      out((long long)"long long ");
      print_p((*(long long*)((uintptr_t)((gl_name + (i * 8))))));
      out((long long)" = 0;");
      (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
    }
    i = (i + 1);
  }
  if ((gl_cnt > 0)) {
    (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
  }
  return 0;
  return 0;
}

uint8_t emit_type_for(long long ty) {
  if ((ty == TY_FLOAT)) {
    out((long long)"double");
    return 0;
  }
  if ((ty == TY_BYTE)) {
    out((long long)"uint8_t");
    return 0;
  }
  if ((ty == TY_QUART)) {
    out((long long)"uint16_t");
    return 0;
  }
  if ((ty == TY_HALF)) {
    out((long long)"uint32_t");
    return 0;
  }
  out((long long)"long long");
  return 0;
  return 0;
}

long long emit_get_par_hint(long long fi, long long j) {
  long long fn_rec, hints, h;
  fn_rec = arr_get(funcs, fi);
  hints = (*(long long*)((uintptr_t)((fn_rec + 48))));
  if ((hints == 0)) {
    return 0;
  }
  h = (*(long long*)((uintptr_t)((hints + (j * 8)))));
  return h;
  return 0;
}

uint8_t emit_forward_decls(void) {
  long long i, j, arr, cnt, rty, pty, pi;
  i = 0;
  while ((i < fn_cnt)) {
    pi = (*(long long*)((uintptr_t)((fn_fidx + (i * 8)))));
    rty = get_fn_ret_type(pi);
    if ((rty == TY_UNKNOWN)) {
      rty = TY_WORD;
    }
    emit_type_for(rty);
    out((long long)" ");
    emit_fname((*(long long*)((uintptr_t)((fn_name + (i * 8))))));
    out((long long)"(");
    arr = (*(long long*)((uintptr_t)((fn_par + (i * 8)))));
    cnt = (*(long long*)((uintptr_t)((fn_pcnt + (i * 8)))));
    if ((cnt == 0)) {
      out((long long)"void");
    } else {
      j = 0;
      while ((j < cnt)) {
        long long ph;
        if ((j > 0)) {
          out((long long)", ");
        }
        ph = emit_get_par_hint(pi, j);
        if ((ph > 0)) {
          pty = ph;
        } else {
          pty = get_fn_par_type(pi, j);
          if ((pty == TY_UNKNOWN)) {
            pty = TY_WORD;
          }
        }
        emit_type_for(pty);
        out((long long)" ");
        print_p((*(long long*)((uintptr_t)((arr + (j * 8))))));
        j = (j + 1);
      }
    }
    out((long long)");");
    (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
    i = (i + 1);
  }
  (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
  return 0;
  return 0;
}

uint8_t emit_functions(void) {
  long long i, j, arr, cnt, rty, pty, pi;
  i = 0;
  while ((i < fn_cnt)) {
    pi = (*(long long*)((uintptr_t)((fn_fidx + (i * 8)))));
    cur_fn_idx = pi;
    rty = get_fn_ret_type(pi);
    if ((rty == TY_UNKNOWN)) {
      rty = TY_WORD;
    }
    emit_type_for(rty);
    out((long long)" ");
    emit_fname((*(long long*)((uintptr_t)((fn_name + (i * 8))))));
    out((long long)"(");
    arr = (*(long long*)((uintptr_t)((fn_par + (i * 8)))));
    cnt = (*(long long*)((uintptr_t)((fn_pcnt + (i * 8)))));
    if ((cnt == 0)) {
      out((long long)"void");
    } else {
      j = 0;
      while ((j < cnt)) {
        long long ph;
        if ((j > 0)) {
          out((long long)", ");
        }
        ph = emit_get_par_hint(pi, j);
        if ((ph > 0)) {
          pty = ph;
        } else {
          pty = get_fn_par_type(pi, j);
          if ((pty == TY_UNKNOWN)) {
            pty = TY_WORD;
          }
        }
        emit_type_for(pty);
        out((long long)" ");
        print_p((*(long long*)((uintptr_t)((arr + (j * 8))))));
        j = (j + 1);
      }
    }
    out((long long)") {");
    (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
    g_indent = 1;
    emit_body((*(long long*)((uintptr_t)((fn_body + (i * 8))))), (*(long long*)((uintptr_t)((fn_bcnt + (i * 8))))));
    g_indent = 0;
    out((long long)"  return 0;");
    (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
    out((long long)"}");
    (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
    (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
    i = (i + 1);
  }
  return 0;
  return 0;
}

uint8_t emit_main_entry(void) {
  long long i, has_main;
  has_main = 0;
  i = 0;
  while ((i < fn_cnt)) {
    if (str_eq((*(long long*)((uintptr_t)((fn_name + (i * 8))))), (long long)"main", 4)) {
      has_main = 1;
    }
    i = (i + 1);
  }
  out((long long)"int main(int argc, char** argv) {");
  (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
  out((long long)"  _bpp_argc = argc;");
  (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
  out((long long)"  _bpp_argv = argv;");
  (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
  if (has_main) {
    out((long long)"  main_bpp();");
    (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
  }
  out((long long)"  return 0;");
  (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
  out((long long)"}");
  (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
  return 0;
  return 0;
}

uint8_t emit_all(void) {
  bridge_data();
  find_used_externs();
  emit_includes();
  emit_mem_model();
  emit_runtime();
  emit_globals();
  emit_forward_decls();
  emit_functions();
  emit_main_entry();
  return 0;
  return 0;
}

uint8_t init_enc_arm64(void) {
  enc_buf = (long long)malloc(1048576);
  enc_pos = 0;
  enc_lbl_off = arr_new();
  enc_lbl_max = 0;
  enc_fix_pos = arr_new();
  enc_fix_lbl = arr_new();
  enc_fix_ty = arr_new();
  enc_fix_cnt = 0;
  enc_rel_pos = arr_new();
  enc_rel_sym = arr_new();
  enc_rel_ty = arr_new();
  enc_rel_cnt = 0;
  return 0;
  return 0;
}

uint8_t enc_emit32(long long val) {
  (*(uint8_t*)((uintptr_t)((enc_buf + enc_pos))) = (uint8_t)((val & 0xFF)));
  (*(uint8_t*)((uintptr_t)(((enc_buf + enc_pos) + 1))) = (uint8_t)(((val >> 8) & 0xFF)));
  (*(uint8_t*)((uintptr_t)(((enc_buf + enc_pos) + 2))) = (uint8_t)(((val >> 16) & 0xFF)));
  (*(uint8_t*)((uintptr_t)(((enc_buf + enc_pos) + 3))) = (uint8_t)(((val >> 24) & 0xFF)));
  enc_pos = (enc_pos + 4);
  return 0;
  return 0;
}

uint8_t enc_patch32(long long off, long long val) {
  (*(uint8_t*)((uintptr_t)((enc_buf + off))) = (uint8_t)((val & 0xFF)));
  (*(uint8_t*)((uintptr_t)(((enc_buf + off) + 1))) = (uint8_t)(((val >> 8) & 0xFF)));
  (*(uint8_t*)((uintptr_t)(((enc_buf + off) + 2))) = (uint8_t)(((val >> 16) & 0xFF)));
  (*(uint8_t*)((uintptr_t)(((enc_buf + off) + 3))) = (uint8_t)(((val >> 24) & 0xFF)));
  return 0;
  return 0;
}

long long enc_read32(long long off) {
  return (((((long long)(*(uint8_t*)((uintptr_t)((enc_buf + off))))) | (((long long)(*(uint8_t*)((uintptr_t)(((enc_buf + off) + 1))))) << 8)) | (((long long)(*(uint8_t*)((uintptr_t)(((enc_buf + off) + 2))))) << 16)) | (((long long)(*(uint8_t*)((uintptr_t)(((enc_buf + off) + 3))))) << 24));
  return 0;
}

uint8_t enc_def_label(long long id) {
  arr_set(enc_lbl_off, id, enc_pos);
  if ((id >= enc_lbl_max)) {
    enc_lbl_max = (id + 1);
  }
  return 0;
  return 0;
}

uint8_t enc_def_label_at(long long id, long long off) {
  arr_set(enc_lbl_off, id, off);
  return 0;
  return 0;
}

long long enc_new_label(void) {
  long long id;
  id = enc_lbl_max;
  enc_lbl_off = arr_push(enc_lbl_off, (0 - 1));
  enc_lbl_max = (enc_lbl_max + 1);
  return id;
  return 0;
}

uint8_t enc_add_fixup(long long lbl, long long ty) {
  enc_fix_pos = arr_push(enc_fix_pos, enc_pos);
  enc_fix_lbl = arr_push(enc_fix_lbl, lbl);
  enc_fix_ty = arr_push(enc_fix_ty, ty);
  enc_fix_cnt = (enc_fix_cnt + 1);
  return 0;
  return 0;
}

uint8_t enc_add_reloc(long long pos, long long sym, long long ty) {
  enc_rel_pos = arr_push(enc_rel_pos, pos);
  enc_rel_sym = arr_push(enc_rel_sym, sym);
  enc_rel_ty = arr_push(enc_rel_ty, ty);
  enc_rel_cnt = (enc_rel_cnt + 1);
  return 0;
  return 0;
}

uint8_t enc_movz(long long rd, long long imm16, long long hw) {
  enc_emit32((((0xD2800000 | (hw << 21)) | (imm16 << 5)) | rd));
  return 0;
  return 0;
}

uint8_t enc_movk(long long rd, long long imm16, long long hw) {
  enc_emit32((((0xF2800000 | (hw << 21)) | (imm16 << 5)) | rd));
  return 0;
  return 0;
}

uint8_t enc_movn(long long rd, long long imm16, long long hw) {
  enc_emit32((((0x92800000 | (hw << 21)) | (imm16 << 5)) | rd));
  return 0;
  return 0;
}

uint8_t enc_mov_wide(long long rd, long long val) {
  long long lo, hi;
  if ((val == (-1))) {
    enc_movn(rd, 0, 0);
    return 0;
  }
  if ((val >= 0)) {
    if ((val < 65536)) {
      enc_movz(rd, val, 0);
      return 0;
    }
  }
  lo = (val & 0xFFFF);
  enc_movz(rd, lo, 0);
  hi = ((val >> 16) & 0xFFFF);
  if ((hi > 0)) {
    enc_movk(rd, hi, 1);
  }
  hi = ((val >> 32) & 0xFFFF);
  if ((hi > 0)) {
    enc_movk(rd, hi, 2);
  }
  hi = ((val >> 48) & 0xFFFF);
  if ((hi > 0)) {
    enc_movk(rd, hi, 3);
  }
  return 0;
  return 0;
}

uint8_t enc_add_reg(long long rd, long long rn, long long rm) {
  enc_emit32((((0x8B000000 | (rm << 16)) | (rn << 5)) | rd));
  return 0;
  return 0;
}

uint8_t enc_sub_reg(long long rd, long long rn, long long rm) {
  enc_emit32((((0xCB000000 | (rm << 16)) | (rn << 5)) | rd));
  return 0;
  return 0;
}

uint8_t enc_and_reg(long long rd, long long rn, long long rm) {
  enc_emit32((((0x8A000000 | (rm << 16)) | (rn << 5)) | rd));
  return 0;
  return 0;
}

uint8_t enc_orr_reg(long long rd, long long rn, long long rm) {
  enc_emit32((((0xAA000000 | (rm << 16)) | (rn << 5)) | rd));
  return 0;
  return 0;
}

uint8_t enc_eor_reg(long long rd, long long rn, long long rm) {
  enc_emit32((((0xCA000000 | (rm << 16)) | (rn << 5)) | rd));
  return 0;
  return 0;
}

uint8_t enc_cmp_reg(long long rn, long long rm) {
  enc_emit32((((0xEB000000 | (rm << 16)) | (rn << 5)) | 0x1F));
  return 0;
  return 0;
}

uint8_t enc_mvn(long long rd, long long rm) {
  enc_emit32((((0xAA200000 | (rm << 16)) | (31 << 5)) | rd));
  return 0;
  return 0;
}

uint8_t enc_neg(long long rd, long long rm) {
  enc_emit32((((0xCB000000 | (rm << 16)) | (31 << 5)) | rd));
  return 0;
  return 0;
}

uint8_t enc_mov_reg(long long rd, long long rm) {
  enc_emit32((((0xAA000000 | (rm << 16)) | (31 << 5)) | rd));
  return 0;
  return 0;
}

uint8_t enc_mul(long long rd, long long rn, long long rm) {
  enc_emit32((((0x9B007C00 | (rm << 16)) | (rn << 5)) | rd));
  return 0;
  return 0;
}

uint8_t enc_sdiv(long long rd, long long rn, long long rm) {
  enc_emit32((((0x9AC00C00 | (rm << 16)) | (rn << 5)) | rd));
  return 0;
  return 0;
}

uint8_t enc_msub(long long rd, long long rn, long long rm, long long ra) {
  enc_emit32(((((0x9B008000 | (rm << 16)) | (ra << 10)) | (rn << 5)) | rd));
  return 0;
  return 0;
}

uint8_t enc_lslv(long long rd, long long rn, long long rm) {
  enc_emit32((((0x9AC02000 | (rm << 16)) | (rn << 5)) | rd));
  return 0;
  return 0;
}

uint8_t enc_asrv(long long rd, long long rn, long long rm) {
  enc_emit32((((0x9AC02800 | (rm << 16)) | (rn << 5)) | rd));
  return 0;
  return 0;
}

uint8_t enc_lsrv(long long rd, long long rn, long long rm) {
  enc_emit32((((0x9AC02400 | (rm << 16)) | (rn << 5)) | rd));
  return 0;
  return 0;
}

uint8_t enc_add_imm(long long rd, long long rn, long long imm12) {
  enc_emit32((((0x91000000 | (imm12 << 10)) | (rn << 5)) | rd));
  return 0;
  return 0;
}

uint8_t enc_sub_imm(long long rd, long long rn, long long imm12) {
  enc_emit32((((0xD1000000 | (imm12 << 10)) | (rn << 5)) | rd));
  return 0;
  return 0;
}

uint8_t enc_subs_imm(long long rd, long long rn, long long imm12) {
  enc_emit32((((0xF1000000 | (imm12 << 10)) | (rn << 5)) | rd));
  return 0;
  return 0;
}

uint8_t enc_cmp_imm(long long rn, long long imm12) {
  enc_emit32((((0xF1000000 | (imm12 << 10)) | (rn << 5)) | 0x1F));
  return 0;
  return 0;
}

uint8_t enc_cset(long long rd, long long cond) {
  enc_emit32(((0x9A9F07E0 | ((cond ^ 1) << 12)) | rd));
  return 0;
  return 0;
}

uint8_t enc_csneg(long long rd, long long rn, long long rm, long long cond) {
  enc_emit32(((((0xDA800400 | (rm << 16)) | (cond << 12)) | (rn << 5)) | rd));
  return 0;
  return 0;
}

uint8_t enc_ldr_uoff(long long rt, long long rn, long long uoff) {
  enc_emit32((((0xF9400000 | ((uoff / 8) << 10)) | (rn << 5)) | rt));
  return 0;
  return 0;
}

uint8_t enc_str_uoff(long long rt, long long rn, long long uoff) {
  enc_emit32((((0xF9000000 | ((uoff / 8) << 10)) | (rn << 5)) | rt));
  return 0;
  return 0;
}

uint8_t enc_ldrb_uoff(long long rt, long long rn, long long uoff) {
  enc_emit32((((0x39400000 | (uoff << 10)) | (rn << 5)) | rt));
  return 0;
  return 0;
}

uint8_t enc_strb_uoff(long long rt, long long rn, long long uoff) {
  enc_emit32((((0x39000000 | (uoff << 10)) | (rn << 5)) | rt));
  return 0;
  return 0;
}

uint8_t enc_ldrh_uoff(long long rt, long long rn, long long uoff) {
  enc_emit32((((0x79400000 | ((uoff / 2) << 10)) | (rn << 5)) | rt));
  return 0;
  return 0;
}

uint8_t enc_strh_uoff(long long rt, long long rn, long long uoff) {
  enc_emit32((((0x79000000 | ((uoff / 2) << 10)) | (rn << 5)) | rt));
  return 0;
  return 0;
}

uint8_t enc_ldr_w_uoff(long long rt, long long rn, long long uoff) {
  enc_emit32((((0xB9400000 | ((uoff / 4) << 10)) | (rn << 5)) | rt));
  return 0;
  return 0;
}

uint8_t enc_str_w_uoff(long long rt, long long rn, long long uoff) {
  enc_emit32((((0xB9000000 | ((uoff / 4) << 10)) | (rn << 5)) | rt));
  return 0;
  return 0;
}

uint8_t enc_ldrb_reg(long long rt, long long rn, long long rm) {
  enc_emit32((((0x38606800 | (rm << 16)) | (rn << 5)) | rt));
  return 0;
  return 0;
}

uint8_t enc_ldrb_post(long long rt, long long rn, long long simm) {
  enc_emit32((((0x38400400 | ((simm & 0x1FF) << 12)) | (rn << 5)) | rt));
  return 0;
  return 0;
}

uint8_t enc_strb_post(long long rt, long long rn, long long simm) {
  enc_emit32((((0x38000400 | ((simm & 0x1FF) << 12)) | (rn << 5)) | rt));
  return 0;
  return 0;
}

uint8_t enc_str_pre(long long rt, long long rn, long long simm) {
  enc_emit32((((0xF8000C00 | ((simm & 0x1FF) << 12)) | (rn << 5)) | rt));
  return 0;
  return 0;
}

uint8_t enc_ldr_post(long long rt, long long rn, long long simm) {
  enc_emit32((((0xF8400400 | ((simm & 0x1FF) << 12)) | (rn << 5)) | rt));
  return 0;
  return 0;
}

uint8_t enc_str_d_pre(long long rt, long long rn, long long simm) {
  enc_emit32((((0xFC000C00 | ((simm & 0x1FF) << 12)) | (rn << 5)) | rt));
  return 0;
  return 0;
}

uint8_t enc_ldr_d_post(long long rt, long long rn, long long simm) {
  enc_emit32((((0xFC400400 | ((simm & 0x1FF) << 12)) | (rn << 5)) | rt));
  return 0;
  return 0;
}

uint8_t enc_stp_pre(long long rt1, long long rt2, long long rn, long long simm) {
  enc_emit32(((((0xA9800000 | (((simm / 8) & 0x7F) << 15)) | (rt2 << 10)) | (rn << 5)) | rt1));
  return 0;
  return 0;
}

uint8_t enc_ldp_post(long long rt1, long long rt2, long long rn, long long simm) {
  enc_emit32(((((0xA8C00000 | (((simm / 8) & 0x7F) << 15)) | (rt2 << 10)) | (rn << 5)) | rt1));
  return 0;
  return 0;
}

uint8_t enc_b(long long off) {
  enc_emit32((0x14000000 | ((off / 4) & 0x3FFFFFF)));
  return 0;
  return 0;
}

uint8_t enc_bl(long long off) {
  enc_emit32((0x94000000 | ((off / 4) & 0x3FFFFFF)));
  return 0;
  return 0;
}

uint8_t enc_cbz(long long rt, long long off) {
  enc_emit32(((0xB4000000 | (((off / 4) & 0x7FFFF) << 5)) | rt));
  return 0;
  return 0;
}

uint8_t enc_cbnz(long long rt, long long off) {
  enc_emit32(((0xB5000000 | (((off / 4) & 0x7FFFF) << 5)) | rt));
  return 0;
  return 0;
}

uint8_t enc_b_cond(long long cond, long long off) {
  enc_emit32(((0x54000000 | (((off / 4) & 0x7FFFF) << 5)) | cond));
  return 0;
  return 0;
}

uint8_t enc_b_label(long long lbl) {
  long long target;
  target = arr_get(enc_lbl_off, lbl);
  if ((target >= 0)) {
    enc_b((target - enc_pos));
    return 0;
  }
  enc_add_fixup(lbl, 0);
  enc_emit32(0x14000000);
  return 0;
  return 0;
}

uint8_t enc_bl_label(long long lbl) {
  long long target;
  target = arr_get(enc_lbl_off, lbl);
  if ((target >= 0)) {
    enc_bl((target - enc_pos));
    return 0;
  }
  enc_add_fixup(lbl, 0);
  enc_emit32(0x94000000);
  return 0;
  return 0;
}

uint8_t enc_cbz_label(long long rt, long long lbl) {
  long long target;
  target = arr_get(enc_lbl_off, lbl);
  if ((target >= 0)) {
    enc_cbz(rt, (target - enc_pos));
    return 0;
  }
  enc_add_fixup(lbl, 1);
  enc_emit32((0xB4000000 | rt));
  return 0;
  return 0;
}

uint8_t enc_cbnz_label(long long rt, long long lbl) {
  long long target;
  target = arr_get(enc_lbl_off, lbl);
  if ((target >= 0)) {
    enc_cbnz(rt, (target - enc_pos));
    return 0;
  }
  enc_add_fixup(lbl, 1);
  enc_emit32((0xB5000000 | rt));
  return 0;
  return 0;
}

uint8_t enc_b_cond_label(long long cond, long long lbl) {
  long long target;
  target = arr_get(enc_lbl_off, lbl);
  if ((target >= 0)) {
    enc_b_cond(cond, (target - enc_pos));
    return 0;
  }
  enc_add_fixup(lbl, 2);
  enc_emit32((0x54000000 | cond));
  return 0;
  return 0;
}

uint8_t enc_blr(long long rn) {
  enc_emit32((0xD63F0000 | (rn << 5)));
  return 0;
  return 0;
}

uint8_t enc_ret(void) {
  enc_emit32(0xD65F03C0);
  return 0;
  return 0;
}

uint8_t enc_svc(long long imm16) {
  enc_emit32((0xD4000001 | (imm16 << 5)));
  return 0;
  return 0;
}

uint8_t enc_nop(void) {
  enc_emit32(0xD503201F);
  return 0;
  return 0;
}

uint8_t enc_fadd(long long rd, long long rn, long long rm) {
  enc_emit32((((0x1E602800 | (rm << 16)) | (rn << 5)) | rd));
  return 0;
  return 0;
}

uint8_t enc_fsub(long long rd, long long rn, long long rm) {
  enc_emit32((((0x1E603800 | (rm << 16)) | (rn << 5)) | rd));
  return 0;
  return 0;
}

uint8_t enc_fmul(long long rd, long long rn, long long rm) {
  enc_emit32((((0x1E600800 | (rm << 16)) | (rn << 5)) | rd));
  return 0;
  return 0;
}

uint8_t enc_fdiv(long long rd, long long rn, long long rm) {
  enc_emit32((((0x1E601800 | (rm << 16)) | (rn << 5)) | rd));
  return 0;
  return 0;
}

uint8_t enc_fcmp(long long rn, long long rm) {
  enc_emit32(((0x1E602000 | (rm << 16)) | (rn << 5)));
  return 0;
  return 0;
}

uint8_t enc_fcmp_zero(long long rn) {
  enc_emit32((0x1E602008 | (rn << 5)));
  return 0;
  return 0;
}

uint8_t enc_fmov(long long rd, long long rn) {
  enc_emit32(((0x1E604000 | (rn << 5)) | rd));
  return 0;
  return 0;
}

uint8_t enc_fneg(long long rd, long long rn) {
  enc_emit32(((0x1E614000 | (rn << 5)) | rd));
  return 0;
  return 0;
}

uint8_t enc_scvtf(long long rd, long long rn) {
  enc_emit32(((0x9E620000 | (rn << 5)) | rd));
  return 0;
  return 0;
}

uint8_t enc_fcvtzs(long long rd, long long rn) {
  enc_emit32(((0x9E780000 | (rn << 5)) | rd));
  return 0;
  return 0;
}

uint8_t enc_fmov_to_gpr(long long rd, long long rn) {
  enc_emit32(((0x9E660000 | (rn << 5)) | rd));
  return 0;
  return 0;
}

uint8_t enc_fmov_from_gpr(long long rd, long long rn) {
  enc_emit32(((0x9E670000 | (rn << 5)) | rd));
  return 0;
  return 0;
}

uint8_t enc_ldr_d_uoff(long long rt, long long rn, long long uoff) {
  enc_emit32((((0xFD400000 | ((uoff / 8) << 10)) | (rn << 5)) | rt));
  return 0;
  return 0;
}

uint8_t enc_str_d_uoff(long long rt, long long rn, long long uoff) {
  enc_emit32((((0xFD000000 | ((uoff / 8) << 10)) | (rn << 5)) | rt));
  return 0;
  return 0;
}

uint8_t enc_ldr_reg_shift3(long long rt, long long rn, long long rm) {
  enc_emit32((((0xF8607800 | (rm << 16)) | (rn << 5)) | rt));
  return 0;
  return 0;
}

uint8_t enc_adrp(long long rd) {
  enc_emit32((0x90000000 | rd));
  return 0;
  return 0;
}

uint8_t enc_adr_label(long long rd, long long lbl) {
  long long target, off, immlo, immhi;
  target = arr_get(enc_lbl_off, lbl);
  if ((target >= 0)) {
    off = (target - enc_pos);
    immlo = (off & 3);
    immhi = ((off >> 2) & 0x7FFFF);
    enc_emit32((((0x10000000 | (immlo << 29)) | (immhi << 5)) | rd));
    return 0;
  }
  enc_add_fixup(lbl, 3);
  enc_emit32((0x10000000 | rd));
  return 0;
  return 0;
}

uint8_t enc_resolve_fixups(void) {
  long long i, pos, lbl, ty, target, off, ins;
  i = 0;
  while ((i < enc_fix_cnt)) {
    pos = arr_get(enc_fix_pos, i);
    lbl = arr_get(enc_fix_lbl, i);
    ty = arr_get(enc_fix_ty, i);
    target = arr_get(enc_lbl_off, lbl);
    off = ((target - pos) / 4);
    ins = enc_read32(pos);
    if ((ty == 0)) {
      ins = ((ins & 0xFC000000) | (off & 0x3FFFFFF));
    }
    if (((ty == 1) | (ty == 2))) {
      ins = ((ins & 0xFF00001F) | ((off & 0x7FFFF) << 5));
    }
    if ((ty == 3)) {
      off = (target - pos);
      ins = (ins & 0x9F00001F);
      ins = ((ins | ((off & 3) << 29)) | (((off >> 2) & 0x7FFFF) << 5));
    }
    enc_patch32(pos, ins);
    i = (i + 1);
  }
  return 0;
  return 0;
}

uint8_t init_codegen_arm64(void) {
  a64_fn_name = arr_new();
  a64_fn_par = arr_new();
  a64_fn_pcnt = arr_new();
  a64_fn_body = arr_new();
  a64_fn_bcnt = arr_new();
  a64_fn_fidx = arr_new();
  a64_gl_name = arr_new();
  a64_ex_name = arr_new();
  a64_ex_ret = arr_new();
  a64_ex_args = arr_new();
  a64_ex_acnt = arr_new();
  a64_vars = arr_new();
  a64_var_stack = arr_new();
  a64_var_struct_idx = arr_new();
  a64_var_off = arr_new();
  a64_var_forced_ty = arr_new();
  a64_flt_tbl = arr_new();
  a64_str_tbl = arr_new();
  a64_lbl_cnt = 0;
  a64_depth = 0;
  a64_break_stack = arr_new();
  a64_bin_mode = 0;
  a64_fn_lbl = arr_new();
  a64_ret_lbl = 0;
  a64_argc_sym = (0 - 1);
  a64_argv_sym = (0 - 2);
  return 0;
  return 0;
}

uint8_t bridge_data_arm64(void) {
  long long i, rec, name_p, idx, j;
  a64_sbuf = vbuf;
  arr_clear(a64_fn_name);
  arr_clear(a64_fn_par);
  arr_clear(a64_fn_pcnt);
  arr_clear(a64_fn_body);
  arr_clear(a64_fn_bcnt);
  arr_clear(a64_fn_fidx);
  i = 0;
  while ((i < func_cnt)) {
    rec = (*(long long*)((uintptr_t)((funcs + (i * 8)))));
    name_p = (*(long long*)((uintptr_t)((rec + FN_NAME))));
    idx = (-1);
    j = 0;
    while ((j < arr_len(a64_fn_name))) {
      if (a64_str_eq_packed(name_p, arr_get(a64_fn_name, j))) {
        idx = j;
      }
      j = (j + 1);
    }
    if ((idx < 0)) {
      idx = arr_len(a64_fn_name);
      a64_fn_name = arr_push(a64_fn_name, name_p);
      a64_fn_par = arr_push(a64_fn_par, (*(long long*)((uintptr_t)((rec + FN_PARS)))));
      a64_fn_pcnt = arr_push(a64_fn_pcnt, (*(long long*)((uintptr_t)((rec + FN_PCNT)))));
      a64_fn_body = arr_push(a64_fn_body, (*(long long*)((uintptr_t)((rec + FN_BODY)))));
      a64_fn_bcnt = arr_push(a64_fn_bcnt, (*(long long*)((uintptr_t)((rec + FN_BCNT)))));
      a64_fn_fidx = arr_push(a64_fn_fidx, i);
    } else {
      arr_set(a64_fn_name, idx, name_p);
      arr_set(a64_fn_par, idx, (*(long long*)((uintptr_t)((rec + FN_PARS)))));
      arr_set(a64_fn_pcnt, idx, (*(long long*)((uintptr_t)((rec + FN_PCNT)))));
      arr_set(a64_fn_body, idx, (*(long long*)((uintptr_t)((rec + FN_BODY)))));
      arr_set(a64_fn_bcnt, idx, (*(long long*)((uintptr_t)((rec + FN_BCNT)))));
      arr_set(a64_fn_fidx, idx, i);
    }
    i = (i + 1);
  }
  arr_clear(a64_gl_name);
  i = 0;
  while ((i < glob_cnt)) {
    name_p = (*(long long*)((uintptr_t)((globals + (i * 8)))));
    idx = (-1);
    j = 0;
    while ((j < arr_len(a64_gl_name))) {
      if (a64_str_eq_packed(name_p, arr_get(a64_gl_name, j))) {
        idx = j;
      }
      j = (j + 1);
    }
    if ((idx < 0)) {
      a64_gl_name = arr_push(a64_gl_name, name_p);
    }
    i = (i + 1);
  }
  arr_clear(a64_ex_name);
  arr_clear(a64_ex_ret);
  arr_clear(a64_ex_args);
  arr_clear(a64_ex_acnt);
  i = 0;
  while ((i < ext_cnt)) {
    rec = (*(long long*)((uintptr_t)((externs + (i * 8)))));
    a64_ex_name = arr_push(a64_ex_name, (*(long long*)((uintptr_t)((rec + EX_NAME)))));
    a64_ex_ret = arr_push(a64_ex_ret, (*(long long*)((uintptr_t)((rec + EX_RET)))));
    a64_ex_args = arr_push(a64_ex_args, (*(long long*)((uintptr_t)((rec + EX_ARGS)))));
    a64_ex_acnt = arr_push(a64_ex_acnt, (*(long long*)((uintptr_t)((rec + EX_ACNT)))));
    i = (i + 1);
  }
  return 0;
  return 0;
}

uint8_t a64_find_ext(long long name_p) {
  long long i;
  i = 0;
  while ((i < arr_len(a64_ex_name))) {
    if (a64_str_eq_packed(name_p, arr_get(a64_ex_name, i))) {
      return i;
    }
    i = (i + 1);
  }
  return (0 - 1);
  return 0;
}

uint8_t a64_find_ext_by_argc(long long name_p, long long argc) {
  long long i;
  i = 0;
  while ((i < arr_len(a64_ex_name))) {
    if (a64_str_eq_packed(name_p, arr_get(a64_ex_name, i))) {
      if ((arr_get(a64_ex_acnt, i) == argc)) {
        return i;
      }
    }
    i = (i + 1);
  }
  return a64_find_ext(name_p);
  return 0;
}

long long a64_ext_par_is_float(long long exi, long long par_idx) {
  long long arr, tp;
  arr = arr_get(a64_ex_args, exi);
  tp = (*(long long*)((uintptr_t)((arr + (par_idx * 8)))));
  return a64_str_eq(tp, (long long)"double", 6);
  return 0;
}

long long a64_ext_ret_is_float(long long exi) {
  long long tp;
  tp = arr_get(a64_ex_ret, exi);
  return a64_str_eq(tp, (long long)"double", 6);
  return 0;
}

uint8_t a64_str_eq(long long p, long long lit, long long litlen) {
  if ((unpack_l(p) != litlen)) {
    return 0;
  }
  return buf_eq((a64_sbuf + unpack_s(p)), lit, litlen);
  return 0;
}

uint8_t a64_str_eq_packed(long long a, long long b) {
  long long as, al, bs, bl, i;
  al = unpack_l(a);
  bl = unpack_l(b);
  if ((al != bl)) {
    return 0;
  }
  as = unpack_s(a);
  bs = unpack_s(b);
  i = 0;
  while ((i < al)) {
    if ((((long long)(*(uint8_t*)((uintptr_t)(((a64_sbuf + as) + i))))) != ((long long)(*(uint8_t*)((uintptr_t)(((a64_sbuf + bs) + i))))))) {
      return 0;
    }
    i = (i + 1);
  }
  return 1;
  return 0;
}

uint8_t a64_print_p(long long p) {
  print_buf((a64_sbuf + unpack_s(p)), unpack_l(p));
  return 0;
  return 0;
}

uint8_t a64_print_dec(long long n) {
  long long buf, len, i;
  if ((n < 0)) {
    (_bpp_scratch[0] = (uint8_t)(45), bpp_sys_write(1, _bpp_scratch, 1));
    n = (0 - n);
  }
  if ((n == 0)) {
    (_bpp_scratch[0] = (uint8_t)(48), bpp_sys_write(1, _bpp_scratch, 1));
    return 0;
  }
  buf = (long long)malloc(20);
  len = 0;
  while ((n > 0)) {
    (*(uint8_t*)((uintptr_t)((buf + len))) = (uint8_t)((48 + (n % 10))));
    len = (len + 1);
    n = (n / 10);
  }
  i = (len - 1);
  while ((i >= 0)) {
    (_bpp_scratch[0] = (uint8_t)(((long long)(*(uint8_t*)((uintptr_t)((buf + i)))))), bpp_sys_write(1, _bpp_scratch, 1));
    i = (i - 1);
  }
  return 0;
  return 0;
}

long long a64_var_add(long long name_p) {
  long long idx;
  idx = arr_len(a64_vars);
  a64_vars = arr_push(a64_vars, name_p);
  a64_var_stack = arr_push(a64_var_stack, 0);
  a64_var_struct_idx = arr_push(a64_var_struct_idx, (0 - 1));
  a64_var_off = arr_push(a64_var_off, 0);
  a64_var_forced_ty = arr_push(a64_var_forced_ty, 0);
  return idx;
  return 0;
}

uint8_t a64_var_idx(long long name_p) {
  long long i;
  i = 0;
  while ((i < arr_len(a64_vars))) {
    if (a64_str_eq_packed(name_p, arr_get(a64_vars, i))) {
      return i;
    }
    i = (i + 1);
  }
  return (-1);
  return 0;
}

long long a64_var_find(long long name_p) {
  long long i;
  i = a64_var_idx(name_p);
  if ((i >= 0)) {
    return arr_get(a64_var_off, i);
  }
  return (-1);
  return 0;
}

uint8_t a64_pre_reg_vars(long long arr, long long cnt) {
  long long i, j;
  long long n;
  i = 0;
  while ((i < cnt)) {
    n = (*(long long*)((uintptr_t)((arr + (i * 8)))));
    if ((n != 0)) {
      if (((*(long long*)((uintptr_t)((n + 0)))) == T_DECL)) {
        j = 0;
        while ((j < (*(long long*)((uintptr_t)((n + 16)))))) {
          long long vi;
          vi = a64_var_add((*(long long*)((uintptr_t)(((*(long long*)((uintptr_t)((n + 8)))) + (j * 8))))));
          if (((*(long long*)((uintptr_t)((n + 32)))) == 1)) {
            arr_set(a64_var_stack, vi, 1);
            arr_set(a64_var_struct_idx, vi, get_var_type((*(long long*)((uintptr_t)(((*(long long*)((uintptr_t)((n + 8)))) + (j * 8)))))));
          }
          if (((*(long long*)((uintptr_t)((n + 24)))) > 0)) {
            arr_set(a64_var_forced_ty, vi, (*(long long*)((uintptr_t)((n + 24)))));
          }
          j = (j + 1);
        }
      }
      if (((*(long long*)((uintptr_t)((n + 0)))) == T_IF)) {
        a64_pre_reg_vars((*(long long*)((uintptr_t)((n + 16)))), (*(long long*)((uintptr_t)((n + 24)))));
        if (((*(long long*)((uintptr_t)((n + 40)))) > 0)) {
          a64_pre_reg_vars((*(long long*)((uintptr_t)((n + 32)))), (*(long long*)((uintptr_t)((n + 40)))));
        }
      }
      if (((*(long long*)((uintptr_t)((n + 0)))) == T_WHILE)) {
        a64_pre_reg_vars((*(long long*)((uintptr_t)((n + 16)))), (*(long long*)((uintptr_t)((n + 24)))));
      }
    }
    i = (i + 1);
  }
  return 0;
  return 0;
}

long long a64_parse_int(long long p) {
  long long s, l, i, ch, val;
  s = unpack_s(p);
  l = unpack_l(p);
  val = 0;
  i = 0;
  if ((l > 2)) {
    if ((((long long)(*(uint8_t*)((uintptr_t)((a64_sbuf + s))))) == 48)) {
      if (((((long long)(*(uint8_t*)((uintptr_t)(((a64_sbuf + s) + 1))))) == 120) | (((long long)(*(uint8_t*)((uintptr_t)(((a64_sbuf + s) + 1))))) == 88))) {
        i = 2;
        while ((i < l)) {
          ch = ((long long)(*(uint8_t*)((uintptr_t)(((a64_sbuf + s) + i)))));
          val = (val * 16);
          if ((ch >= 48)) {
            if ((ch <= 57)) {
              val = ((val + ch) - 48);
            }
          }
          if ((ch >= 65)) {
            if ((ch <= 70)) {
              val = ((val + ch) - 55);
            }
          }
          if ((ch >= 97)) {
            if ((ch <= 102)) {
              val = ((val + ch) - 87);
            }
          }
          i = (i + 1);
        }
        return val;
      }
    }
  }
  while ((i < l)) {
    ch = ((long long)(*(uint8_t*)((uintptr_t)(((a64_sbuf + s) + i)))));
    val = (((val * 10) + ch) - 48);
    i = (i + 1);
  }
  return val;
  return 0;
}

uint8_t a64_is_float_lit(long long p) {
  long long s, l, i;
  s = unpack_s(p);
  l = unpack_l(p);
  i = 0;
  while ((i < l)) {
    if ((((long long)(*(uint8_t*)((uintptr_t)(((a64_sbuf + s) + i))))) == 46)) {
      return 1;
    }
    i = (i + 1);
  }
  return 0;
  return 0;
}

long long a64_var_type(long long vi) {
  return get_fn_var_type(a64_cur_fn_idx, arr_get(a64_vars, vi));
  return 0;
}

long long a64_var_is_float(long long vi) {
  return (a64_var_type(vi) == TY_FLOAT);
  return 0;
}

uint8_t a64_emit_load_var(long long vi, long long off) {
  long long fty;
  fty = arr_get(a64_var_forced_ty, vi);
  if (a64_var_is_float(vi)) {
    if (a64_bin_mode) {
      enc_ldr_d_uoff(0, 29, off);
    } else {
      out((long long)"  ldr d0, [x29, #");
      a64_print_dec(off);
      out((long long)"]");
      (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
    }
    return 1;
  }
  if ((fty == TY_BYTE)) {
    if (a64_bin_mode) {
      enc_ldrb_uoff(0, 29, off);
    } else {
      out((long long)"  ldrb w0, [x29, #");
      a64_print_dec(off);
      out((long long)"]");
      (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
    }
    return 0;
  }
  if ((fty == TY_QUART)) {
    if (a64_bin_mode) {
      enc_ldrh_uoff(0, 29, off);
    } else {
      out((long long)"  ldrh w0, [x29, #");
      a64_print_dec(off);
      out((long long)"]");
      (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
    }
    return 0;
  }
  if ((fty == TY_HALF)) {
    if (a64_bin_mode) {
      enc_ldr_w_uoff(0, 29, off);
    } else {
      out((long long)"  ldr w0, [x29, #");
      a64_print_dec(off);
      out((long long)"]");
      (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
    }
    return 0;
  }
  if (a64_bin_mode) {
    enc_ldr_uoff(0, 29, off);
  } else {
    out((long long)"  ldr x0, [x29, #");
    a64_print_dec(off);
    out((long long)"]");
    (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
  }
  return 0;
  return 0;
}

uint8_t a64_emit_store_var(long long vi, long long off, long long rty) {
  long long fty;
  fty = arr_get(a64_var_forced_ty, vi);
  if (a64_var_is_float(vi)) {
    if ((rty == 0)) {
      if (a64_bin_mode) {
        enc_scvtf(0, 0);
      } else {
        out((long long)"  scvtf d0, x0");
        (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
      }
    }
    if (a64_bin_mode) {
      enc_str_d_uoff(0, 29, off);
    } else {
      out((long long)"  str d0, [x29, #");
      a64_print_dec(off);
      out((long long)"]");
      (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
    }
    return 0;
  }
  if ((rty == 1)) {
    if (a64_bin_mode) {
      enc_fcvtzs(0, 0);
    } else {
      out((long long)"  fcvtzs x0, d0");
      (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
    }
  }
  if ((fty == TY_BYTE)) {
    if (a64_bin_mode) {
      enc_strb_uoff(0, 29, off);
    } else {
      out((long long)"  strb w0, [x29, #");
      a64_print_dec(off);
      out((long long)"]");
      (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
    }
    return 0;
  }
  if ((fty == TY_QUART)) {
    if (a64_bin_mode) {
      enc_strh_uoff(0, 29, off);
    } else {
      out((long long)"  strh w0, [x29, #");
      a64_print_dec(off);
      out((long long)"]");
      (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
    }
    return 0;
  }
  if ((fty == TY_HALF)) {
    if (a64_bin_mode) {
      enc_str_w_uoff(0, 29, off);
    } else {
      out((long long)"  str w0, [x29, #");
      a64_print_dec(off);
      out((long long)"]");
      (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
    }
    return 0;
  }
  if (a64_bin_mode) {
    enc_str_uoff(0, 29, off);
  } else {
    out((long long)"  str x0, [x29, #");
    a64_print_dec(off);
    out((long long)"]");
    (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
  }
  return 0;
  return 0;
}

uint8_t a64_find_fn(long long name_p) {
  long long i;
  i = 0;
  while ((i < arr_len(a64_fn_name))) {
    if (a64_str_eq_packed(name_p, arr_get(a64_fn_name, i))) {
      return i;
    }
    i = (i + 1);
  }
  return (-1);
  return 0;
}

long long a64_new_lbl(void) {
  long long l;
  l = a64_lbl_cnt;
  a64_lbl_cnt = (a64_lbl_cnt + 1);
  return l;
  return 0;
}

uint8_t a64_emit_mov(long long val) {
  long long lo, hi;
  if (a64_bin_mode) {
    enc_mov_wide(0, val);
    return 0;
  }
  if ((val >= 0)) {
    if ((val < 65536)) {
      out((long long)"  mov x0, #");
      a64_print_dec(val);
      (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
      return 0;
    }
  }
  lo = (val & 0xFFFF);
  out((long long)"  movz x0, #");
  a64_print_dec(lo);
  (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
  hi = ((val >> 16) & 0xFFFF);
  if ((hi > 0)) {
    out((long long)"  movk x0, #");
    a64_print_dec(hi);
    out((long long)", lsl #16");
    (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
  }
  hi = ((val >> 32) & 0xFFFF);
  if ((hi > 0)) {
    out((long long)"  movk x0, #");
    a64_print_dec(hi);
    out((long long)", lsl #32");
    (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
  }
  hi = ((val >> 48) & 0xFFFF);
  if ((hi > 0)) {
    out((long long)"  movk x0, #");
    a64_print_dec(hi);
    out((long long)", lsl #48");
    (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
  }
  return 0;
  return 0;
}

uint8_t a64_emit_push(void) {
  if (a64_bin_mode) {
    enc_str_pre(0, 31, (-16));
  } else {
    out((long long)"  str x0, [sp, #-16]!");
    (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
  }
  a64_depth = (a64_depth + 1);
  return 0;
  return 0;
}

uint8_t a64_emit_pop(long long r) {
  if (a64_bin_mode) {
    enc_ldr_post(r, 31, 16);
  } else {
    out((long long)"  ldr x");
    a64_print_dec(r);
    out((long long)", [sp], #16");
    (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
  }
  a64_depth = (a64_depth - 1);
  return 0;
  return 0;
}

uint8_t a64_emit_fpush(void) {
  if (a64_bin_mode) {
    enc_str_d_pre(0, 31, (-16));
  } else {
    out((long long)"  str d0, [sp, #-16]!");
    (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
  }
  a64_depth = (a64_depth + 1);
  return 0;
  return 0;
}

uint8_t a64_emit_fpop(long long r) {
  if (a64_bin_mode) {
    enc_ldr_d_post(r, 31, 16);
  } else {
    out((long long)"  ldr d");
    a64_print_dec(r);
    out((long long)", [sp], #16");
    (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
  }
  a64_depth = (a64_depth - 1);
  return 0;
  return 0;
}

uint8_t a64_emit_node(long long nd) {
  long long n;
  long long t, op_s, op_l, c0, c1, arr, cnt, i, off, lbl, lbl2, vi, lty, rty;
  long long lit_s, lit_l, sid, str_p, fid, use_flt, is_io, uty, ety, fidx_c, fi, fn_name;
  if ((nd == 0)) {
    if (a64_bin_mode) {
      enc_movz(0, 0, 0);
    } else {
      out((long long)"  mov x0, #0");
      (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
    }
    return 0;
  }
  n = nd;
  t = (*(long long*)((uintptr_t)((n + 0))));
  if ((t == T_LIT)) {
    lit_s = unpack_s((*(long long*)((uintptr_t)((n + 8)))));
    lit_l = unpack_l((*(long long*)((uintptr_t)((n + 8)))));
    if ((((long long)(*(uint8_t*)((uintptr_t)((a64_sbuf + lit_s))))) == 34)) {
      sid = arr_len(a64_str_tbl);
      str_p = pack((lit_s + 1), (lit_l - 2));
      a64_str_tbl = arr_push(a64_str_tbl, str_p);
      if (a64_bin_mode) {
        enc_add_reloc(enc_pos, sid, 1);
        enc_adrp(0);
        enc_add_imm(0, 0, 0);
      } else {
        out((long long)"  adrp x0, _str_");
        a64_print_dec(sid);
        out((long long)"@PAGE");
        (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
        out((long long)"  add x0, x0, _str_");
        a64_print_dec(sid);
        out((long long)"@PAGEOFF");
        (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
      }
      return 0;
    }
    if ((a64_is_float_lit((*(long long*)((uintptr_t)((n + 8))))) == 1)) {
      fid = arr_len(a64_flt_tbl);
      a64_flt_tbl = arr_push(a64_flt_tbl, (*(long long*)((uintptr_t)((n + 8)))));
      if (a64_bin_mode) {
        enc_add_reloc(enc_pos, fid, 2);
        enc_adrp(8);
        enc_add_imm(8, 8, 0);
        enc_ldr_d_uoff(0, 8, 0);
      } else {
        out((long long)"  adrp x8, _flt_");
        a64_print_dec(fid);
        out((long long)"@PAGE");
        (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
        out((long long)"  add x8, x8, _flt_");
        a64_print_dec(fid);
        out((long long)"@PAGEOFF");
        (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
        out((long long)"  ldr d0, [x8]");
        (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
      }
      return 1;
    }
    a64_emit_mov(a64_parse_int((*(long long*)((uintptr_t)((n + 8))))));
    return 0;
  }
  if ((t == T_VAR)) {
    vi = a64_var_idx((*(long long*)((uintptr_t)((n + 8)))));
    if ((vi >= 0)) {
      off = arr_get(a64_var_off, vi);
      if (arr_get(a64_var_stack, vi)) {
        if (a64_bin_mode) {
          enc_add_imm(0, 29, off);
        } else {
          out((long long)"  add x0, x29, #");
          a64_print_dec(off);
          (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
        }
        return 0;
      }
      return a64_emit_load_var(vi, off);
    }
    if ((ty_get_global_type((*(long long*)((uintptr_t)((n + 8))))) == TY_FLOAT)) {
      if (a64_bin_mode) {
        enc_add_reloc(enc_pos, (*(long long*)((uintptr_t)((n + 8)))), 0);
        enc_adrp(8);
        enc_add_imm(8, 8, 0);
        enc_ldr_d_uoff(0, 8, 0);
      } else {
        out((long long)"  adrp x8, _");
        a64_print_p((*(long long*)((uintptr_t)((n + 8)))));
        out((long long)"@PAGE");
        (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
        out((long long)"  add x8, x8, _");
        a64_print_p((*(long long*)((uintptr_t)((n + 8)))));
        out((long long)"@PAGEOFF");
        (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
        out((long long)"  ldr d0, [x8]");
        (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
      }
      return 1;
    }
    if (a64_bin_mode) {
      enc_add_reloc(enc_pos, (*(long long*)((uintptr_t)((n + 8)))), 0);
      enc_adrp(8);
      enc_add_imm(8, 8, 0);
      enc_ldr_uoff(0, 8, 0);
    } else {
      out((long long)"  adrp x8, _");
      a64_print_p((*(long long*)((uintptr_t)((n + 8)))));
      out((long long)"@PAGE");
      (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
      out((long long)"  add x8, x8, _");
      a64_print_p((*(long long*)((uintptr_t)((n + 8)))));
      out((long long)"@PAGEOFF");
      (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
      out((long long)"  ldr x0, [x8]");
      (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
    }
    return 0;
  }
  if ((t == T_BINOP)) {
    op_s = unpack_s((*(long long*)((uintptr_t)((n + 8)))));
    op_l = unpack_l((*(long long*)((uintptr_t)((n + 8)))));
    c0 = ((long long)(*(uint8_t*)((uintptr_t)((a64_sbuf + op_s)))));
    c1 = 0;
    if ((op_l > 1)) {
      c1 = ((long long)(*(uint8_t*)((uintptr_t)(((a64_sbuf + op_s) + 1)))));
    }
    is_io = 0;
    if ((c0 == 37)) {
      is_io = 1;
    }
    if ((c0 == 94)) {
      is_io = 1;
    }
    if ((c0 == 38)) {
      if ((op_l == 1)) {
        is_io = 1;
      }
    }
    if ((c0 == 124)) {
      if ((op_l == 1)) {
        is_io = 1;
      }
    }
    if ((c0 == 60)) {
      if ((c1 == 60)) {
        is_io = 1;
      }
    }
    if ((c0 == 62)) {
      if ((c1 == 62)) {
        is_io = 1;
      }
    }
    lty = a64_emit_node((*(long long*)((uintptr_t)((n + 16)))));
    if ((lty == 1)) {
      a64_emit_fpush();
    } else {
      a64_emit_push();
    }
    rty = a64_emit_node((*(long long*)((uintptr_t)((n + 24)))));
    use_flt = 0;
    if ((is_io == 0)) {
      if (((lty == 1) | (rty == 1))) {
        use_flt = 1;
      }
    }
    if ((use_flt == 1)) {
      if ((rty == 0)) {
        if (a64_bin_mode) {
          enc_scvtf(0, 0);
        } else {
          out((long long)"  scvtf d0, x0");
          (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
        }
      }
      if ((lty == 1)) {
        a64_emit_fpop(1);
      } else {
        a64_emit_pop(1);
        if (a64_bin_mode) {
          enc_scvtf(1, 1);
        } else {
          out((long long)"  scvtf d1, x1");
          (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
        }
      }
      if ((c0 == 43)) {
        if ((op_l == 1)) {
          if (a64_bin_mode) {
            enc_fadd(0, 1, 0);
          } else {
            out((long long)"  fadd d0, d1, d0");
            (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
          }
          return 1;
        }
      }
      if ((c0 == 45)) {
        if ((op_l == 1)) {
          if (a64_bin_mode) {
            enc_fsub(0, 1, 0);
          } else {
            out((long long)"  fsub d0, d1, d0");
            (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
          }
          return 1;
        }
      }
      if ((c0 == 42)) {
        if ((op_l == 1)) {
          if (a64_bin_mode) {
            enc_fmul(0, 1, 0);
          } else {
            out((long long)"  fmul d0, d1, d0");
            (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
          }
          return 1;
        }
      }
      if ((c0 == 47)) {
        if ((op_l == 1)) {
          if (a64_bin_mode) {
            enc_fdiv(0, 1, 0);
          } else {
            out((long long)"  fdiv d0, d1, d0");
            (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
          }
          return 1;
        }
      }
      if ((c0 == 61)) {
        if ((c1 == 61)) {
          if (a64_bin_mode) {
            enc_fcmp(1, 0);
            enc_cset(0, 0);
          } else {
            out((long long)"  fcmp d1, d0");
            (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
            out((long long)"  cset x0, eq");
            (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
          }
          return 0;
        }
      }
      if ((c0 == 33)) {
        if ((c1 == 61)) {
          if (a64_bin_mode) {
            enc_fcmp(1, 0);
            enc_cset(0, 1);
          } else {
            out((long long)"  fcmp d1, d0");
            (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
            out((long long)"  cset x0, ne");
            (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
          }
          return 0;
        }
      }
      if ((c0 == 60)) {
        if ((c1 == 61)) {
          if (a64_bin_mode) {
            enc_fcmp(1, 0);
            enc_cset(0, 9);
          } else {
            out((long long)"  fcmp d1, d0");
            (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
            out((long long)"  cset x0, ls");
            (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
          }
          return 0;
        }
        if ((op_l == 1)) {
          if (a64_bin_mode) {
            enc_fcmp(1, 0);
            enc_cset(0, 4);
          } else {
            out((long long)"  fcmp d1, d0");
            (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
            out((long long)"  cset x0, mi");
            (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
          }
          return 0;
        }
      }
      if ((c0 == 62)) {
        if ((c1 == 61)) {
          if (a64_bin_mode) {
            enc_fcmp(1, 0);
            enc_cset(0, 10);
          } else {
            out((long long)"  fcmp d1, d0");
            (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
            out((long long)"  cset x0, ge");
            (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
          }
          return 0;
        }
        if ((op_l == 1)) {
          if (a64_bin_mode) {
            enc_fcmp(1, 0);
            enc_cset(0, 12);
          } else {
            out((long long)"  fcmp d1, d0");
            (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
            out((long long)"  cset x0, gt");
            (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
          }
          return 0;
        }
      }
      if ((c0 == 38)) {
        if ((c1 == 38)) {
          if (a64_bin_mode) {
            enc_fcmp_zero(1);
            enc_cset(1, 1);
            enc_fcmp_zero(0);
            enc_cset(0, 1);
            enc_and_reg(0, 1, 0);
          } else {
            out((long long)"  fcmp d1, #0.0");
            (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
            out((long long)"  cset x1, ne");
            (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
            out((long long)"  fcmp d0, #0.0");
            (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
            out((long long)"  cset x0, ne");
            (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
            out((long long)"  and x0, x1, x0");
            (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
          }
          return 0;
        }
      }
      if ((c0 == 124)) {
        if ((c1 == 124)) {
          if (a64_bin_mode) {
            enc_fcmp_zero(1);
            enc_cset(1, 1);
            enc_fcmp_zero(0);
            enc_cset(0, 1);
            enc_orr_reg(0, 1, 0);
          } else {
            out((long long)"  fcmp d1, #0.0");
            (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
            out((long long)"  cset x1, ne");
            (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
            out((long long)"  fcmp d0, #0.0");
            (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
            out((long long)"  cset x0, ne");
            (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
            out((long long)"  orr x0, x1, x0");
            (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
          }
          return 0;
        }
      }
      return 1;
    }
    if ((rty == 1)) {
      if (a64_bin_mode) {
        enc_fcvtzs(0, 0);
      } else {
        out((long long)"  fcvtzs x0, d0");
        (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
      }
    }
    if ((lty == 1)) {
      a64_emit_fpop(0);
      if (a64_bin_mode) {
        enc_fcvtzs(1, 0);
      } else {
        out((long long)"  fcvtzs x1, d0");
        (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
      }
    } else {
      a64_emit_pop(1);
    }
    if ((c0 == 43)) {
      if ((op_l == 1)) {
        if (a64_bin_mode) {
          enc_add_reg(0, 1, 0);
        } else {
          out((long long)"  add x0, x1, x0");
          (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
        }
        return 0;
      }
    }
    if ((c0 == 45)) {
      if ((op_l == 1)) {
        if (a64_bin_mode) {
          enc_sub_reg(0, 1, 0);
        } else {
          out((long long)"  sub x0, x1, x0");
          (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
        }
        return 0;
      }
    }
    if ((c0 == 42)) {
      if ((op_l == 1)) {
        if (a64_bin_mode) {
          enc_mul(0, 1, 0);
        } else {
          out((long long)"  mul x0, x1, x0");
          (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
        }
        return 0;
      }
    }
    if ((c0 == 47)) {
      if ((op_l == 1)) {
        if (a64_bin_mode) {
          enc_sdiv(0, 1, 0);
        } else {
          out((long long)"  sdiv x0, x1, x0");
          (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
        }
        return 0;
      }
    }
    if ((c0 == 37)) {
      if ((op_l == 1)) {
        if (a64_bin_mode) {
          enc_sdiv(8, 1, 0);
          enc_msub(0, 8, 0, 1);
        } else {
          out((long long)"  sdiv x8, x1, x0");
          (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
          out((long long)"  msub x0, x8, x0, x1");
          (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
        }
        return 0;
      }
    }
    if ((c0 == 38)) {
      if ((op_l == 1)) {
        if (a64_bin_mode) {
          enc_and_reg(0, 1, 0);
        } else {
          out((long long)"  and x0, x1, x0");
          (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
        }
        return 0;
      }
    }
    if ((c0 == 124)) {
      if ((op_l == 1)) {
        if (a64_bin_mode) {
          enc_orr_reg(0, 1, 0);
        } else {
          out((long long)"  orr x0, x1, x0");
          (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
        }
        return 0;
      }
    }
    if ((c0 == 94)) {
      if ((op_l == 1)) {
        if (a64_bin_mode) {
          enc_eor_reg(0, 1, 0);
        } else {
          out((long long)"  eor x0, x1, x0");
          (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
        }
        return 0;
      }
    }
    if ((c0 == 62)) {
      if ((c1 == 62)) {
        if (a64_bin_mode) {
          enc_asrv(0, 1, 0);
        } else {
          out((long long)"  asr x0, x1, x0");
          (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
        }
        return 0;
      }
    }
    if ((c0 == 60)) {
      if ((c1 == 60)) {
        if (a64_bin_mode) {
          enc_lslv(0, 1, 0);
        } else {
          out((long long)"  lsl x0, x1, x0");
          (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
        }
        return 0;
      }
    }
    if ((c0 == 61)) {
      if ((c1 == 61)) {
        if (a64_bin_mode) {
          enc_cmp_reg(1, 0);
          enc_cset(0, 0);
        } else {
          out((long long)"  cmp x1, x0");
          (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
          out((long long)"  cset x0, eq");
          (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
        }
        return 0;
      }
    }
    if ((c0 == 33)) {
      if ((c1 == 61)) {
        if (a64_bin_mode) {
          enc_cmp_reg(1, 0);
          enc_cset(0, 1);
        } else {
          out((long long)"  cmp x1, x0");
          (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
          out((long long)"  cset x0, ne");
          (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
        }
        return 0;
      }
    }
    if ((c0 == 60)) {
      if ((c1 == 61)) {
        if (a64_bin_mode) {
          enc_cmp_reg(1, 0);
          enc_cset(0, 13);
        } else {
          out((long long)"  cmp x1, x0");
          (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
          out((long long)"  cset x0, le");
          (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
        }
        return 0;
      }
      if ((op_l == 1)) {
        if (a64_bin_mode) {
          enc_cmp_reg(1, 0);
          enc_cset(0, 11);
        } else {
          out((long long)"  cmp x1, x0");
          (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
          out((long long)"  cset x0, lt");
          (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
        }
        return 0;
      }
    }
    if ((c0 == 62)) {
      if ((c1 == 61)) {
        if (a64_bin_mode) {
          enc_cmp_reg(1, 0);
          enc_cset(0, 10);
        } else {
          out((long long)"  cmp x1, x0");
          (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
          out((long long)"  cset x0, ge");
          (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
        }
        return 0;
      }
      if ((op_l == 1)) {
        if (a64_bin_mode) {
          enc_cmp_reg(1, 0);
          enc_cset(0, 12);
        } else {
          out((long long)"  cmp x1, x0");
          (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
          out((long long)"  cset x0, gt");
          (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
        }
        return 0;
      }
    }
    if ((c0 == 38)) {
      if ((c1 == 38)) {
        if (a64_bin_mode) {
          enc_cmp_imm(1, 0);
          enc_cset(1, 1);
          enc_cmp_imm(0, 0);
          enc_cset(0, 1);
          enc_and_reg(0, 1, 0);
        } else {
          out((long long)"  cmp x1, #0");
          (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
          out((long long)"  cset x1, ne");
          (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
          out((long long)"  cmp x0, #0");
          (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
          out((long long)"  cset x0, ne");
          (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
          out((long long)"  and x0, x1, x0");
          (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
        }
        return 0;
      }
    }
    if ((c0 == 124)) {
      if ((c1 == 124)) {
        if (a64_bin_mode) {
          enc_cmp_imm(1, 0);
          enc_cset(1, 1);
          enc_cmp_imm(0, 0);
          enc_cset(0, 1);
          enc_orr_reg(0, 1, 0);
        } else {
          out((long long)"  cmp x1, #0");
          (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
          out((long long)"  cset x1, ne");
          (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
          out((long long)"  cmp x0, #0");
          (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
          out((long long)"  cset x0, ne");
          (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
          out((long long)"  orr x0, x1, x0");
          (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
        }
        return 0;
      }
    }
    return 0;
  }
  if ((t == T_UNARY)) {
    uty = a64_emit_node((*(long long*)((uintptr_t)((n + 16)))));
    op_s = unpack_s((*(long long*)((uintptr_t)((n + 8)))));
    op_l = unpack_l((*(long long*)((uintptr_t)((n + 8)))));
    if ((op_l == 1)) {
      if ((((long long)(*(uint8_t*)((uintptr_t)((a64_sbuf + op_s))))) == 126)) {
        if (a64_bin_mode) {
          enc_mvn(0, 0);
        } else {
          out((long long)"  mvn x0, x0");
          (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
        }
        return 0;
      }
    }
    if ((uty == 1)) {
      if (a64_bin_mode) {
        enc_fneg(0, 0);
      } else {
        out((long long)"  fneg d0, d0");
        (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
      }
      return 1;
    }
    if (a64_bin_mode) {
      enc_neg(0, 0);
    } else {
      out((long long)"  neg x0, x0");
      (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
    }
    return 0;
  }
  if ((t == T_CALL)) {
    if (a64_str_eq((*(long long*)((uintptr_t)((n + 8)))), (long long)"putchar", 7)) {
      arr = (*(long long*)((uintptr_t)((n + 16))));
      ety = a64_emit_node((*(long long*)((uintptr_t)((arr + 0)))));
      if ((ety == 1)) {
        if (a64_bin_mode) {
          enc_fcvtzs(0, 0);
        } else {
          out((long long)"  fcvtzs x0, d0");
          (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
        }
      }
      if (a64_bin_mode) {
        enc_strb_uoff(0, 29, 16);
        enc_mov_wide(0, 1);
        enc_add_imm(1, 29, 16);
        enc_mov_wide(2, 1);
        enc_mov_wide(16, 4);
        enc_svc(0x80);
        enc_csneg(0, 0, 0, 3);
      } else {
        out((long long)"  strb w0, [x29, #16]");
        (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
        out((long long)"  mov x0, #1");
        (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
        out((long long)"  add x1, x29, #16");
        (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
        out((long long)"  mov x2, #1");
        (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
        out((long long)"  mov x16, #4");
        (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
        out((long long)"  svc #0x80");
        (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
        out((long long)"  csneg x0, x0, x0, cc");
        (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
      }
      return 0;
    }
    if (a64_str_eq((*(long long*)((uintptr_t)((n + 8)))), (long long)"getchar", 7)) {
      if (a64_bin_mode) {
        lbl = enc_new_label();
        lbl2 = enc_new_label();
        enc_strb_uoff(31, 29, 16);
        enc_mov_wide(0, 0);
        enc_add_imm(1, 29, 16);
        enc_mov_wide(2, 1);
        enc_mov_wide(16, 3);
        enc_svc(0x80);
        enc_csneg(0, 0, 0, 3);
        enc_cmp_imm(0, 1);
        enc_b_cond_label(11, lbl);
        enc_ldrb_uoff(0, 29, 16);
        enc_b_label(lbl2);
        enc_def_label(lbl);
        enc_mov_wide(0, (-1));
        enc_def_label(lbl2);
      } else {
        lbl = a64_new_lbl();
        out((long long)"  strb wzr, [x29, #16]");
        (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
        out((long long)"  mov x0, #0");
        (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
        out((long long)"  add x1, x29, #16");
        (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
        out((long long)"  mov x2, #1");
        (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
        out((long long)"  mov x16, #3");
        (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
        out((long long)"  svc #0x80");
        (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
        out((long long)"  csneg x0, x0, x0, cc");
        (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
        out((long long)"  cmp x0, #1");
        (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
        out((long long)"  b.lt .Leof");
        a64_print_dec(lbl);
        (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
        out((long long)"  ldrb w0, [x29, #16]");
        (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
        out((long long)"  b .Leofend");
        a64_print_dec(lbl);
        (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
        out((long long)".Leof");
        a64_print_dec(lbl);
        out((long long)":");
        (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
        out((long long)"  mov x0, #-1");
        (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
        out((long long)".Leofend");
        a64_print_dec(lbl);
        out((long long)":");
        (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
      }
      return 0;
    }
    if (a64_str_eq((*(long long*)((uintptr_t)((n + 8)))), (long long)"malloc", 6)) {
      arr = (*(long long*)((uintptr_t)((n + 16))));
      ety = a64_emit_node((*(long long*)((uintptr_t)((arr + 0)))));
      if ((ety == 1)) {
        if (a64_bin_mode) {
          enc_fcvtzs(0, 0);
        } else {
          out((long long)"  fcvtzs x0, d0");
          (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
        }
      }
      if (a64_bin_mode) {
        enc_mov_reg(1, 0);
        enc_mov_wide(0, 0);
        enc_mov_wide(2, 3);
        enc_mov_wide(3, 0x1002);
        enc_movn(4, 0, 0);
        enc_mov_wide(5, 0);
        enc_mov_wide(16, 197);
        enc_svc(0x80);
        enc_csneg(0, 0, 0, 3);
      } else {
        out((long long)"  bl _malloc");
        (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
      }
      return 0;
    }
    if (a64_str_eq((*(long long*)((uintptr_t)((n + 8)))), (long long)"free", 4)) {
      arr = (*(long long*)((uintptr_t)((n + 16))));
      a64_emit_node((*(long long*)((uintptr_t)((arr + 0)))));
      if (a64_bin_mode) {
        enc_mov_wide(0, 0);
      } else {
        out((long long)"  mov x0, #0");
        (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
      }
      return 0;
    }
    if (a64_str_eq((*(long long*)((uintptr_t)((n + 8)))), (long long)"realloc", 7)) {
      arr = (*(long long*)((uintptr_t)((n + 16))));
      a64_emit_node((*(long long*)((uintptr_t)((arr + 0)))));
      a64_emit_push();
      a64_emit_node((*(long long*)((uintptr_t)((arr + 8)))));
      a64_emit_push();
      a64_emit_node((*(long long*)((uintptr_t)((arr + 16)))));
      if (a64_bin_mode) {
        enc_mov_reg(1, 0);
        enc_mov_wide(0, 0);
        enc_mov_wide(2, 3);
        enc_mov_wide(3, 0x1002);
        enc_movn(4, 0, 0);
        enc_mov_wide(5, 0);
        enc_mov_wide(16, 197);
        enc_svc(0x80);
        enc_csneg(0, 0, 0, 3);
        enc_mov_reg(2, 0);
        a64_emit_pop(3);
        a64_emit_pop(1);
        long long lbl_top, lbl_end;
        lbl_top = enc_new_label();
        lbl_end = enc_new_label();
        enc_cbz_label(3, lbl_end);
        enc_def_label(lbl_top);
        enc_ldrb_post(4, 1, 1);
        enc_strb_post(4, 2, 1);
        enc_subs_imm(3, 3, 1);
        enc_cbnz_label(3, lbl_top);
        enc_def_label(lbl_end);
      } else {
        out((long long)"  bl _realloc");
        (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
        a64_emit_pop(1);
        a64_emit_pop(1);
      }
      return 0;
    }
    if (a64_str_eq((*(long long*)((uintptr_t)((n + 8)))), (long long)"memcpy", 6)) {
      arr = (*(long long*)((uintptr_t)((n + 16))));
      a64_emit_node((*(long long*)((uintptr_t)((arr + 0)))));
      a64_emit_push();
      a64_emit_node((*(long long*)((uintptr_t)((arr + 8)))));
      a64_emit_push();
      a64_emit_node((*(long long*)((uintptr_t)((arr + 16)))));
      if (a64_bin_mode) {
        long long lbl_top, lbl_end;
        enc_mov_reg(3, 0);
        a64_emit_pop(1);
        a64_emit_pop(2);
        lbl_top = enc_new_label();
        lbl_end = enc_new_label();
        enc_cbz_label(3, lbl_end);
        enc_def_label(lbl_top);
        enc_ldrb_post(4, 1, 1);
        enc_strb_post(4, 2, 1);
        enc_subs_imm(3, 3, 1);
        enc_cbnz_label(3, lbl_top);
        enc_def_label(lbl_end);
        enc_mov_reg(0, 2);
      } else {
        out((long long)"  bl _memcpy");
        (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
      }
      return 0;
    }
    if (a64_str_eq((*(long long*)((uintptr_t)((n + 8)))), (long long)"peek", 4)) {
      arr = (*(long long*)((uintptr_t)((n + 16))));
      ety = a64_emit_node((*(long long*)((uintptr_t)((arr + 0)))));
      if ((ety == 1)) {
        if (a64_bin_mode) {
          enc_fcvtzs(0, 0);
        } else {
          out((long long)"  fcvtzs x0, d0");
          (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
        }
      }
      if (a64_bin_mode) {
        enc_ldrb_uoff(0, 0, 0);
      } else {
        out((long long)"  ldrb w0, [x0]");
        (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
      }
      return 0;
    }
    if (a64_str_eq((*(long long*)((uintptr_t)((n + 8)))), (long long)"poke", 4)) {
      arr = (*(long long*)((uintptr_t)((n + 16))));
      ety = a64_emit_node((*(long long*)((uintptr_t)((arr + 8)))));
      if ((ety == 1)) {
        if (a64_bin_mode) {
          enc_fcvtzs(0, 0);
        } else {
          out((long long)"  fcvtzs x0, d0");
          (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
        }
      }
      a64_emit_push();
      ety = a64_emit_node((*(long long*)((uintptr_t)((arr + 0)))));
      if ((ety == 1)) {
        if (a64_bin_mode) {
          enc_fcvtzs(0, 0);
        } else {
          out((long long)"  fcvtzs x0, d0");
          (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
        }
      }
      a64_emit_pop(1);
      if (a64_bin_mode) {
        enc_strb_uoff(1, 0, 0);
      } else {
        out((long long)"  strb w1, [x0]");
        (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
      }
      return 0;
    }
    if (a64_str_eq((*(long long*)((uintptr_t)((n + 8)))), (long long)"float_ret2", 10)) {
      if (a64_bin_mode) {
        enc_fmov(0, 1);
      } else {
        out((long long)"  fmov d0, d1");
        (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
      }
      return 1;
    }
    if (a64_str_eq((*(long long*)((uintptr_t)((n + 8)))), (long long)"str_peek", 8)) {
      arr = (*(long long*)((uintptr_t)((n + 16))));
      ety = a64_emit_node((*(long long*)((uintptr_t)((arr + 8)))));
      if ((ety == 1)) {
        if (a64_bin_mode) {
          enc_fcvtzs(0, 0);
        } else {
          out((long long)"  fcvtzs x0, d0");
          (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
        }
      }
      a64_emit_push();
      ety = a64_emit_node((*(long long*)((uintptr_t)((arr + 0)))));
      if ((ety == 1)) {
        if (a64_bin_mode) {
          enc_fcvtzs(0, 0);
        } else {
          out((long long)"  fcvtzs x0, d0");
          (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
        }
      }
      a64_emit_pop(1);
      if (a64_bin_mode) {
        enc_ldrb_reg(0, 0, 1);
      } else {
        out((long long)"  ldrb w0, [x0, x1]");
        (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
      }
      return 0;
    }
    if (a64_str_eq((*(long long*)((uintptr_t)((n + 8)))), (long long)"sys_write", 9)) {
      arr = (*(long long*)((uintptr_t)((n + 16))));
      ety = a64_emit_node((*(long long*)((uintptr_t)((arr + 16)))));
      if ((ety == 1)) {
        if (a64_bin_mode) {
          enc_fcvtzs(0, 0);
        } else {
          out((long long)"  fcvtzs x0, d0");
          (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
        }
      }
      a64_emit_push();
      ety = a64_emit_node((*(long long*)((uintptr_t)((arr + 8)))));
      if ((ety == 1)) {
        if (a64_bin_mode) {
          enc_fcvtzs(0, 0);
        } else {
          out((long long)"  fcvtzs x0, d0");
          (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
        }
      }
      a64_emit_push();
      ety = a64_emit_node((*(long long*)((uintptr_t)((arr + 0)))));
      if ((ety == 1)) {
        if (a64_bin_mode) {
          enc_fcvtzs(0, 0);
        } else {
          out((long long)"  fcvtzs x0, d0");
          (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
        }
      }
      a64_emit_pop(1);
      a64_emit_pop(2);
      if (a64_bin_mode) {
        enc_mov_wide(16, 4);
        enc_svc(0x80);
        enc_csneg(0, 0, 0, 3);
      } else {
        out((long long)"  mov x16, #4");
        (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
        out((long long)"  svc #0x80");
        (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
        out((long long)"  csneg x0, x0, x0, cc");
        (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
      }
      return 0;
    }
    if (a64_str_eq((*(long long*)((uintptr_t)((n + 8)))), (long long)"sys_read", 8)) {
      arr = (*(long long*)((uintptr_t)((n + 16))));
      ety = a64_emit_node((*(long long*)((uintptr_t)((arr + 16)))));
      if ((ety == 1)) {
        if (a64_bin_mode) {
          enc_fcvtzs(0, 0);
        } else {
          out((long long)"  fcvtzs x0, d0");
          (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
        }
      }
      a64_emit_push();
      ety = a64_emit_node((*(long long*)((uintptr_t)((arr + 8)))));
      if ((ety == 1)) {
        if (a64_bin_mode) {
          enc_fcvtzs(0, 0);
        } else {
          out((long long)"  fcvtzs x0, d0");
          (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
        }
      }
      a64_emit_push();
      ety = a64_emit_node((*(long long*)((uintptr_t)((arr + 0)))));
      if ((ety == 1)) {
        if (a64_bin_mode) {
          enc_fcvtzs(0, 0);
        } else {
          out((long long)"  fcvtzs x0, d0");
          (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
        }
      }
      a64_emit_pop(1);
      a64_emit_pop(2);
      if (a64_bin_mode) {
        enc_mov_wide(16, 3);
        enc_svc(0x80);
        enc_csneg(0, 0, 0, 3);
      } else {
        out((long long)"  mov x16, #3");
        (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
        out((long long)"  svc #0x80");
        (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
        out((long long)"  csneg x0, x0, x0, cc");
        (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
      }
      return 0;
    }
    if (a64_str_eq((*(long long*)((uintptr_t)((n + 8)))), (long long)"sys_open", 8)) {
      arr = (*(long long*)((uintptr_t)((n + 16))));
      ety = a64_emit_node((*(long long*)((uintptr_t)((arr + 8)))));
      if ((ety == 1)) {
        if (a64_bin_mode) {
          enc_fcvtzs(0, 0);
        } else {
          out((long long)"  fcvtzs x0, d0");
          (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
        }
      }
      a64_emit_push();
      ety = a64_emit_node((*(long long*)((uintptr_t)((arr + 0)))));
      if ((ety == 1)) {
        if (a64_bin_mode) {
          enc_fcvtzs(0, 0);
        } else {
          out((long long)"  fcvtzs x0, d0");
          (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
        }
      }
      a64_emit_pop(1);
      if (a64_bin_mode) {
        enc_mov_wide(2, 493);
        enc_mov_wide(16, 5);
        enc_svc(0x80);
        enc_csneg(0, 0, 0, 3);
      } else {
        out((long long)"  mov x2, #493");
        (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
        out((long long)"  mov x16, #5");
        (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
        out((long long)"  svc #0x80");
        (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
        out((long long)"  csneg x0, x0, x0, cc");
        (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
      }
      return 0;
    }
    if (a64_str_eq((*(long long*)((uintptr_t)((n + 8)))), (long long)"sys_close", 9)) {
      arr = (*(long long*)((uintptr_t)((n + 16))));
      ety = a64_emit_node((*(long long*)((uintptr_t)((arr + 0)))));
      if ((ety == 1)) {
        if (a64_bin_mode) {
          enc_fcvtzs(0, 0);
        } else {
          out((long long)"  fcvtzs x0, d0");
          (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
        }
      }
      if (a64_bin_mode) {
        enc_mov_wide(16, 6);
        enc_svc(0x80);
        enc_csneg(0, 0, 0, 3);
      } else {
        out((long long)"  mov x16, #6");
        (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
        out((long long)"  svc #0x80");
        (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
        out((long long)"  csneg x0, x0, x0, cc");
        (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
      }
      return 0;
    }
    if (a64_str_eq((*(long long*)((uintptr_t)((n + 8)))), (long long)"sys_fork", 8)) {
      if (a64_bin_mode) {
        enc_mov_wide(16, 2);
        enc_svc(0x80);
        enc_csneg(0, 0, 0, 3);
      } else {
        out((long long)"  mov x16, #2");
        (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
        out((long long)"  svc #0x80");
        (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
        out((long long)"  csneg x0, x0, x0, cc");
        (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
      }
      return 0;
    }
    if (a64_str_eq((*(long long*)((uintptr_t)((n + 8)))), (long long)"sys_execve", 10)) {
      arr = (*(long long*)((uintptr_t)((n + 16))));
      a64_emit_node((*(long long*)((uintptr_t)((arr + 0)))));
      a64_emit_push();
      a64_emit_node((*(long long*)((uintptr_t)((arr + 8)))));
      a64_emit_push();
      a64_emit_node((*(long long*)((uintptr_t)((arr + 16)))));
      a64_emit_pop(1);
      a64_emit_pop(2);
      if (a64_bin_mode) {
        enc_mov_reg(3, 0);
        enc_mov_reg(0, 2);
        enc_mov_reg(2, 3);
        enc_mov_wide(16, 59);
        enc_svc(0x80);
        enc_csneg(0, 0, 0, 3);
      } else {
        out((long long)"  mov x3, x0");
        (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
        out((long long)"  mov x0, x2");
        (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
        out((long long)"  mov x2, x3");
        (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
        out((long long)"  mov x16, #59");
        (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
        out((long long)"  svc #0x80");
        (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
        out((long long)"  csneg x0, x0, x0, cc");
        (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
      }
      return 0;
    }
    if (a64_str_eq((*(long long*)((uintptr_t)((n + 8)))), (long long)"sys_waitpid", 11)) {
      arr = (*(long long*)((uintptr_t)((n + 16))));
      a64_emit_node((*(long long*)((uintptr_t)((arr + 0)))));
      if (a64_bin_mode) {
        enc_mov_wide(1, 0);
        enc_mov_wide(2, 0);
        enc_mov_wide(3, 0);
        enc_mov_wide(16, 7);
        enc_svc(0x80);
        enc_csneg(0, 0, 0, 3);
      } else {
        out((long long)"  mov x1, #0");
        (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
        out((long long)"  mov x2, #0");
        (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
        out((long long)"  mov x3, #0");
        (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
        out((long long)"  mov x16, #7");
        (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
        out((long long)"  svc #0x80");
        (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
        out((long long)"  csneg x0, x0, x0, cc");
        (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
      }
      return 0;
    }
    if (a64_str_eq((*(long long*)((uintptr_t)((n + 8)))), (long long)"sys_exit", 8)) {
      arr = (*(long long*)((uintptr_t)((n + 16))));
      a64_emit_node((*(long long*)((uintptr_t)((arr + 0)))));
      if (a64_bin_mode) {
        enc_mov_wide(16, 1);
        enc_svc(0x80);
      } else {
        out((long long)"  mov x16, #1");
        (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
        out((long long)"  svc #0x80");
        (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
      }
      return 0;
    }
    if (a64_str_eq((*(long long*)((uintptr_t)((n + 8)))), (long long)"argc_get", 8)) {
      if (a64_bin_mode) {
        enc_add_reloc(enc_pos, a64_argc_sym, 0);
        enc_adrp(8);
        enc_add_imm(8, 8, 0);
        enc_ldr_uoff(0, 8, 0);
      } else {
        out((long long)"  adrp x8, _bpp_argc@PAGE");
        (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
        out((long long)"  add x8, x8, _bpp_argc@PAGEOFF");
        (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
        out((long long)"  ldr x0, [x8]");
        (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
      }
      return 0;
    }
    if (a64_str_eq((*(long long*)((uintptr_t)((n + 8)))), (long long)"argv_get", 8)) {
      a64_emit_node((*(long long*)((uintptr_t)((*(long long*)((uintptr_t)((n + 16))))))));
      a64_emit_push();
      if (a64_bin_mode) {
        enc_add_reloc(enc_pos, a64_argv_sym, 0);
        enc_adrp(8);
        enc_add_imm(8, 8, 0);
        enc_ldr_uoff(0, 8, 0);
        a64_emit_pop(1);
        enc_ldr_reg_shift3(0, 0, 1);
      } else {
        out((long long)"  adrp x8, _bpp_argv@PAGE");
        (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
        out((long long)"  add x8, x8, _bpp_argv@PAGEOFF");
        (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
        out((long long)"  ldr x0, [x8]");
        (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
        a64_emit_pop(1);
        out((long long)"  ldr x0, [x0, x1, lsl #3]");
        (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
      }
      return 0;
    }
    if (a64_str_eq((*(long long*)((uintptr_t)((n + 8)))), (long long)"fn_ptr", 6)) {
      arr = (*(long long*)((uintptr_t)((n + 16))));
      fn_name = (*(long long*)((uintptr_t)((arr + 0))));
      fi = a64_find_fn((*(long long*)((uintptr_t)((fn_name + 8)))));
      if ((fi < 0)) {
        diag_str((long long)"internal error: fn_ptr target not found after validate\n");
        bpp_sys_exit(2);
      }
      if (a64_bin_mode) {
        enc_adr_label(0, arr_get(a64_fn_lbl, fi));
      } else {
        out((long long)"  adrp x0, _");
        a64_print_p((*(long long*)((uintptr_t)((fn_name + 8)))));
        out((long long)"@PAGE");
        (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
        out((long long)"  add x0, x0, _");
        a64_print_p((*(long long*)((uintptr_t)((fn_name + 8)))));
        out((long long)"@PAGEOFF");
        (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
      }
      return 0;
    }
    if (a64_str_eq((*(long long*)((uintptr_t)((n + 8)))), (long long)"call", 4)) {
      arr = (*(long long*)((uintptr_t)((n + 16))));
      cnt = (*(long long*)((uintptr_t)((n + 24))));
      i = 0;
      while ((i < cnt)) {
        a64_emit_node((*(long long*)((uintptr_t)((arr + (i * 8))))));
        a64_emit_push();
        i = (i + 1);
      }
      i = (cnt - 2);
      while ((i >= 0)) {
        a64_emit_pop(i);
        i = (i - 1);
      }
      a64_emit_pop(9);
      if (a64_bin_mode) {
        enc_blr(9);
      } else {
        out((long long)"  blr x9");
        (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
      }
      return 0;
    }
    fidx_c = find_func_idx((*(long long*)((uintptr_t)((n + 8)))));
    arr = (*(long long*)((uintptr_t)((n + 16))));
    cnt = (*(long long*)((uintptr_t)((n + 24))));
    i = 0;
    while ((i < cnt)) {
      ety = a64_emit_node((*(long long*)((uintptr_t)((arr + (i * 8))))));
      if ((fidx_c >= 0)) {
        vi = (get_fn_par_type(fidx_c, i) == TY_FLOAT);
        if ((ety == 1)) {
          if ((vi == 1)) {
            if (a64_bin_mode) {
              enc_fmov_to_gpr(0, 0);
            } else {
              out((long long)"  fmov x0, d0");
              (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
            }
          } else {
            if (a64_bin_mode) {
              enc_fcvtzs(0, 0);
            } else {
              out((long long)"  fcvtzs x0, d0");
              (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
            }
          }
        } else {
          if ((vi == 1)) {
            if (a64_bin_mode) {
              enc_scvtf(0, 0);
              enc_fmov_to_gpr(0, 0);
            } else {
              out((long long)"  scvtf d0, x0");
              (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
              out((long long)"  fmov x0, d0");
              (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
            }
          }
        }
      } else {
        if ((ety == 1)) {
          if (a64_bin_mode) {
            enc_fmov_to_gpr(0, 0);
          } else {
            out((long long)"  fmov x0, d0");
            (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
          }
        }
      }
      a64_emit_push();
      i = (i + 1);
    }
    i = (cnt - 1);
    while ((i >= 0)) {
      a64_emit_pop(i);
      i = (i - 1);
    }
    long long exi;
    exi = a64_find_ext_by_argc((*(long long*)((uintptr_t)((n + 8)))), cnt);
    if ((exi >= 0)) {
      long long int_idx, flt_idx;
      int_idx = 0;
      flt_idx = 0;
      i = 0;
      while ((i < cnt)) {
        if (a64_ext_par_is_float(exi, i)) {
          if (a64_bin_mode) {
            enc_fmov_from_gpr(flt_idx, i);
          } else {
            out((long long)"  fmov d");
            a64_print_dec(flt_idx);
            out((long long)", x");
            a64_print_dec(i);
            (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
          }
          flt_idx = (flt_idx + 1);
        } else {
          if ((int_idx != i)) {
            if (a64_bin_mode) {
              enc_add_imm(int_idx, i, 0);
            } else {
              out((long long)"  mov x");
              a64_print_dec(int_idx);
              out((long long)", x");
              a64_print_dec(i);
              (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
            }
          }
          int_idx = (int_idx + 1);
        }
        i = (i + 1);
      }
    }
    if (a64_bin_mode) {
      fi = a64_find_fn((*(long long*)((uintptr_t)((n + 8)))));
      if ((fi >= 0)) {
        enc_bl_label(arr_get(a64_fn_lbl, fi));
      } else {
        if ((a64_find_ext((*(long long*)((uintptr_t)((n + 8))))) >= 0)) {
          enc_add_reloc(enc_pos, (*(long long*)((uintptr_t)((n + 8)))), 3);
          enc_adrp(16);
          enc_ldr_uoff(16, 16, 0);
          enc_blr(16);
        } else {
          enc_add_reloc(enc_pos, (*(long long*)((uintptr_t)((n + 8)))), 4);
          enc_emit32(0x94000000);
        }
      }
    } else {
      out((long long)"  bl _");
      a64_print_p((*(long long*)((uintptr_t)((n + 8)))));
      (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
    }
    if ((fidx_c >= 0)) {
      if ((get_fn_ret_type(fidx_c) == TY_FLOAT)) {
        return 1;
      }
    }
    if ((exi >= 0)) {
      if (a64_ext_ret_is_float(exi)) {
        return 1;
      }
    }
    return 0;
  }
  if ((t == T_MEMLD)) {
    ety = a64_emit_node((*(long long*)((uintptr_t)((n + 8)))));
    if ((ety == 1)) {
      if (a64_bin_mode) {
        enc_fcvtzs(0, 0);
      } else {
        out((long long)"  fcvtzs x0, d0");
        (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
      }
    }
    if (a64_bin_mode) {
      enc_ldr_uoff(0, 0, 0);
    } else {
      out((long long)"  ldr x0, [x0]");
      (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
    }
    return 0;
  }
  return 0;
  return 0;
}

uint8_t a64_emit_body(long long arr, long long cnt) {
  long long i;
  long long n;
  i = 0;
  while ((i < cnt)) {
    n = (*(long long*)((uintptr_t)((arr + (i * 8)))));
    if ((n != 0)) {
      a64_emit_stmt(n);
    }
    i = (i + 1);
  }
  return 0;
  return 0;
}

uint8_t a64_emit_stmt(long long nd) {
  long long n;
  long long lhs;
  long long t, off, lbl, lbl2, arr, cnt, i, rty, vi, ety;
  if ((nd == 0)) {
    return 0;
  }
  n = nd;
  t = (*(long long*)((uintptr_t)((n + 0))));
  if ((t == T_DECL)) {
    return 0;
  }
  if ((t == T_ASSIGN)) {
    lhs = (*(long long*)((uintptr_t)((n + 8))));
    if (((*(long long*)((uintptr_t)((lhs + 0)))) == T_VAR)) {
      vi = a64_var_idx((*(long long*)((uintptr_t)((lhs + 8)))));
      if ((vi >= 0)) {
        if (arr_get(a64_var_stack, vi)) {
          a64_emit_node((*(long long*)((uintptr_t)((n + 16)))));
          off = arr_get(a64_var_off, vi);
          long long fc, j, sidx;
          sidx = arr_get(a64_var_struct_idx, vi);
          fc = (get_struct_size(sidx) / 8);
          j = 0;
          while ((j < fc)) {
            if (a64_bin_mode) {
              enc_ldr_uoff(8, 0, (j * 8));
              enc_str_uoff(8, 29, (off + (j * 8)));
            } else {
              out((long long)"  ldr x8, [x0, #");
              a64_print_dec((j * 8));
              out((long long)"]");
              (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
              out((long long)"  str x8, [x29, #");
              a64_print_dec((off + (j * 8)));
              out((long long)"]");
              (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
            }
            j = (j + 1);
          }
          return 0;
        }
      }
    }
    rty = a64_emit_node((*(long long*)((uintptr_t)((n + 16)))));
    if (((*(long long*)((uintptr_t)((lhs + 0)))) == T_VAR)) {
      vi = a64_var_idx((*(long long*)((uintptr_t)((lhs + 8)))));
      if ((vi >= 0)) {
        off = arr_get(a64_var_off, vi);
        a64_emit_store_var(vi, off, rty);
      } else {
        if ((ty_get_global_type((*(long long*)((uintptr_t)((lhs + 8))))) == TY_FLOAT)) {
          if ((rty == 0)) {
            if (a64_bin_mode) {
              enc_scvtf(0, 0);
            } else {
              out((long long)"  scvtf d0, x0");
              (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
            }
          }
          if (a64_bin_mode) {
            enc_add_reloc(enc_pos, (*(long long*)((uintptr_t)((lhs + 8)))), 0);
            enc_adrp(8);
            enc_add_imm(8, 8, 0);
            enc_str_d_uoff(0, 8, 0);
          } else {
            out((long long)"  adrp x8, _");
            a64_print_p((*(long long*)((uintptr_t)((lhs + 8)))));
            out((long long)"@PAGE");
            (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
            out((long long)"  add x8, x8, _");
            a64_print_p((*(long long*)((uintptr_t)((lhs + 8)))));
            out((long long)"@PAGEOFF");
            (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
            out((long long)"  str d0, [x8]");
            (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
          }
        } else {
          if ((rty == 1)) {
            if (a64_bin_mode) {
              enc_fcvtzs(0, 0);
            } else {
              out((long long)"  fcvtzs x0, d0");
              (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
            }
          }
          if (a64_bin_mode) {
            enc_add_reloc(enc_pos, (*(long long*)((uintptr_t)((lhs + 8)))), 0);
            enc_adrp(8);
            enc_add_imm(8, 8, 0);
            enc_str_uoff(0, 8, 0);
          } else {
            out((long long)"  adrp x8, _");
            a64_print_p((*(long long*)((uintptr_t)((lhs + 8)))));
            out((long long)"@PAGE");
            (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
            out((long long)"  add x8, x8, _");
            a64_print_p((*(long long*)((uintptr_t)((lhs + 8)))));
            out((long long)"@PAGEOFF");
            (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
            out((long long)"  str x0, [x8]");
            (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
          }
        }
      }
    }
    return 0;
  }
  if ((t == T_MEMST)) {
    ety = a64_emit_node((*(long long*)((uintptr_t)((n + 16)))));
    if ((ety == 1)) {
      if (a64_bin_mode) {
        enc_fmov_to_gpr(0, 0);
      } else {
        out((long long)"  fmov x0, d0");
        (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
      }
    }
    a64_emit_push();
    a64_emit_node((*(long long*)((uintptr_t)((n + 8)))));
    a64_emit_pop(1);
    if (a64_bin_mode) {
      enc_str_uoff(1, 0, 0);
    } else {
      out((long long)"  str x1, [x0]");
      (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
    }
    return 0;
  }
  if ((t == T_IF)) {
    if (a64_bin_mode) {
      lbl = enc_new_label();
      lbl2 = enc_new_label();
      ety = a64_emit_node((*(long long*)((uintptr_t)((n + 8)))));
      if ((ety == 1)) {
        enc_fcmp_zero(0);
        enc_cset(0, 1);
      }
      enc_cbz_label(0, lbl);
      a64_emit_body((*(long long*)((uintptr_t)((n + 16)))), (*(long long*)((uintptr_t)((n + 24)))));
      enc_b_label(lbl2);
      enc_def_label(lbl);
      if (((*(long long*)((uintptr_t)((n + 40)))) > 0)) {
        a64_emit_body((*(long long*)((uintptr_t)((n + 32)))), (*(long long*)((uintptr_t)((n + 40)))));
      }
      enc_def_label(lbl2);
    } else {
      lbl = a64_new_lbl();
      ety = a64_emit_node((*(long long*)((uintptr_t)((n + 8)))));
      if ((ety == 1)) {
        out((long long)"  fcmp d0, #0.0");
        (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
        out((long long)"  cset x0, ne");
        (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
      }
      out((long long)"  cbz x0, .Lelse");
      a64_print_dec(lbl);
      (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
      a64_emit_body((*(long long*)((uintptr_t)((n + 16)))), (*(long long*)((uintptr_t)((n + 24)))));
      out((long long)"  b .Lendif");
      a64_print_dec(lbl);
      (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
      out((long long)".Lelse");
      a64_print_dec(lbl);
      out((long long)":");
      (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
      if (((*(long long*)((uintptr_t)((n + 40)))) > 0)) {
        a64_emit_body((*(long long*)((uintptr_t)((n + 32)))), (*(long long*)((uintptr_t)((n + 40)))));
      }
      out((long long)".Lendif");
      a64_print_dec(lbl);
      out((long long)":");
      (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
    }
    return 0;
  }
  if ((t == T_BREAK)) {
    if ((arr_len(a64_break_stack) > 0)) {
      long long brk_lbl;
      brk_lbl = arr_last(a64_break_stack);
      if (a64_bin_mode) {
        enc_b_label(brk_lbl);
      } else {
        out((long long)"  b .Lend");
        a64_print_dec(brk_lbl);
        (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
      }
    }
    return 0;
  }
  if ((t == T_WHILE)) {
    if (a64_bin_mode) {
      lbl = enc_new_label();
      lbl2 = enc_new_label();
      a64_break_stack = arr_push(a64_break_stack, lbl2);
      enc_def_label(lbl);
      ety = a64_emit_node((*(long long*)((uintptr_t)((n + 8)))));
      if ((ety == 1)) {
        enc_fcmp_zero(0);
        enc_cset(0, 1);
      }
      enc_cbz_label(0, lbl2);
      a64_emit_body((*(long long*)((uintptr_t)((n + 16)))), (*(long long*)((uintptr_t)((n + 24)))));
      enc_b_label(lbl);
      enc_def_label(lbl2);
      arr_pop(a64_break_stack);
    } else {
      lbl = a64_new_lbl();
      a64_break_stack = arr_push(a64_break_stack, lbl);
      out((long long)".Lloop");
      a64_print_dec(lbl);
      out((long long)":");
      (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
      ety = a64_emit_node((*(long long*)((uintptr_t)((n + 8)))));
      if ((ety == 1)) {
        out((long long)"  fcmp d0, #0.0");
        (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
        out((long long)"  cset x0, ne");
        (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
      }
      out((long long)"  cbz x0, .Lend");
      a64_print_dec(lbl);
      (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
      a64_emit_body((*(long long*)((uintptr_t)((n + 16)))), (*(long long*)((uintptr_t)((n + 24)))));
      out((long long)"  b .Lloop");
      a64_print_dec(lbl);
      (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
      out((long long)".Lend");
      a64_print_dec(lbl);
      out((long long)":");
      (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
      arr_pop(a64_break_stack);
    }
    return 0;
  }
  if ((t == T_RET)) {
    rty = a64_emit_node((*(long long*)((uintptr_t)((n + 8)))));
    if ((get_fn_ret_type(a64_cur_fn_idx) == TY_FLOAT)) {
      if ((rty == 0)) {
        if (a64_bin_mode) {
          enc_scvtf(0, 0);
        } else {
          out((long long)"  scvtf d0, x0");
          (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
        }
      }
    } else {
      if ((rty == 1)) {
        if (a64_bin_mode) {
          enc_fcvtzs(0, 0);
        } else {
          out((long long)"  fcvtzs x0, d0");
          (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
        }
      }
    }
    if (a64_bin_mode) {
      enc_b_label(a64_ret_lbl);
    } else {
      out((long long)"  b .Lret_");
      a64_print_p(a64_cur_fn_name);
      (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
    }
    return 0;
  }
  if ((t == T_NOP)) {
    return 0;
  }
  a64_emit_node(nd);
  return 0;
  return 0;
}

uint8_t a64_emit_func(long long fi) {
  long long name_p, par_arr, par_cnt, body_arr, body_cnt;
  long long frame, i;
  name_p = arr_get(a64_fn_name, fi);
  par_arr = arr_get(a64_fn_par, fi);
  par_cnt = arr_get(a64_fn_pcnt, fi);
  body_arr = arr_get(a64_fn_body, fi);
  body_cnt = arr_get(a64_fn_bcnt, fi);
  arr_clear(a64_vars);
  arr_clear(a64_var_stack);
  arr_clear(a64_var_struct_idx);
  arr_clear(a64_var_off);
  arr_clear(a64_var_forced_ty);
  a64_cur_fn_name = name_p;
  a64_cur_fn_idx = arr_get(a64_fn_fidx, fi);
  long long hints, ph, fn_rec;
  fn_rec = arr_get(funcs, a64_cur_fn_idx);
  hints = (*(long long*)((uintptr_t)((fn_rec + 48))));
  i = 0;
  while ((i < par_cnt)) {
    long long pvi;
    pvi = a64_var_add((*(long long*)((uintptr_t)((par_arr + (i * 8))))));
    if ((hints != 0)) {
      ph = (*(long long*)((uintptr_t)((hints + (i * 8)))));
      if ((ph > 0)) {
        arr_set(a64_var_forced_ty, pvi, ph);
      }
    }
    i = (i + 1);
  }
  a64_pre_reg_vars(body_arr, body_cnt);
  long long total, sz;
  total = 0;
  i = 0;
  while ((i < arr_len(a64_vars))) {
    arr_set(a64_var_off, i, (24 + total));
    if (arr_get(a64_var_stack, i)) {
      sz = get_struct_size(arr_get(a64_var_struct_idx, i));
    } else {
      sz = 8;
    }
    total = (total + sz);
    i = (i + 1);
  }
  frame = (24 + total);
  frame = (((frame + 15) / 16) * 16);
  if (a64_bin_mode) {
    enc_def_label(arr_get(a64_fn_lbl, fi));
    a64_ret_lbl = enc_new_label();
    enc_stp_pre(29, 30, 31, (0 - frame));
    enc_add_imm(29, 31, 0);
    if (a64_str_eq(name_p, (long long)"main", 4)) {
      enc_add_reloc(enc_pos, a64_argc_sym, 0);
      enc_adrp(8);
      enc_add_imm(8, 8, 0);
      enc_str_uoff(0, 8, 0);
      enc_add_reloc(enc_pos, a64_argv_sym, 0);
      enc_adrp(8);
      enc_add_imm(8, 8, 0);
      enc_str_uoff(1, 8, 0);
    }
    i = 0;
    while ((i < par_cnt)) {
      enc_str_uoff(i, 29, arr_get(a64_var_off, i));
      i = (i + 1);
    }
    a64_depth = 0;
    a64_emit_body(body_arr, body_cnt);
    enc_def_label(a64_ret_lbl);
    enc_ldp_post(29, 30, 31, frame);
    enc_ret();
  } else {
    out((long long)".p2align 2");
    (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
    if (a64_str_eq(name_p, (long long)"main", 4)) {
      out((long long)".globl _main");
      (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
      out((long long)"_main:");
      (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
    } else {
      out((long long)"_");
      a64_print_p(name_p);
      out((long long)":");
      (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
    }
    out((long long)"  stp x29, x30, [sp, #-");
    a64_print_dec(frame);
    out((long long)"]!");
    (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
    out((long long)"  mov x29, sp");
    (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
    if (a64_str_eq(name_p, (long long)"main", 4)) {
      out((long long)"  adrp x8, _bpp_argc@PAGE");
      (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
      out((long long)"  add x8, x8, _bpp_argc@PAGEOFF");
      (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
      out((long long)"  str x0, [x8]");
      (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
      out((long long)"  adrp x8, _bpp_argv@PAGE");
      (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
      out((long long)"  add x8, x8, _bpp_argv@PAGEOFF");
      (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
      out((long long)"  str x1, [x8]");
      (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
    }
    i = 0;
    while ((i < par_cnt)) {
      out((long long)"  str x");
      a64_print_dec(i);
      out((long long)", [x29, #");
      a64_print_dec(arr_get(a64_var_off, i));
      out((long long)"]");
      (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
      i = (i + 1);
    }
    a64_depth = 0;
    a64_emit_body(body_arr, body_cnt);
    out((long long)".Lret_");
    a64_print_p(name_p);
    out((long long)":");
    (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
    out((long long)"  ldp x29, x30, [sp], #");
    a64_print_dec(frame);
    (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
    out((long long)"  ret");
    (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
    (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
  }
  return 0;
  return 0;
}

uint8_t a64_mod_init(void) {
  long long i, rec, name_p, idx, j;
  a64_sbuf = vbuf;
  enc_pos = 0;
  enc_lbl_max = 0;
  arr_clear(enc_lbl_off);
  enc_fix_cnt = 0;
  arr_clear(enc_fix_pos);
  arr_clear(enc_fix_lbl);
  arr_clear(enc_fix_ty);
  arr_clear(enc_rel_pos);
  arr_clear(enc_rel_sym);
  arr_clear(enc_rel_ty);
  arr_clear(a64_fn_name);
  arr_clear(a64_fn_par);
  arr_clear(a64_fn_pcnt);
  arr_clear(a64_fn_body);
  arr_clear(a64_fn_bcnt);
  arr_clear(a64_fn_fidx);
  arr_clear(a64_fn_lbl);
  arr_clear(a64_gl_name);
  i = 0;
  while ((i < glob_cnt)) {
    name_p = (*(long long*)((uintptr_t)((globals + (i * 8)))));
    idx = (-1);
    j = 0;
    while ((j < arr_len(a64_gl_name))) {
      if (a64_str_eq_packed(name_p, arr_get(a64_gl_name, j))) {
        idx = j;
      }
      j = (j + 1);
    }
    if ((idx < 0)) {
      a64_gl_name = arr_push(a64_gl_name, name_p);
    }
    i = (i + 1);
  }
  a64_gl_name = arr_push(a64_gl_name, a64_argc_sym);
  a64_gl_name = arr_push(a64_gl_name, a64_argv_sym);
  arr_clear(a64_ex_name);
  arr_clear(a64_ex_ret);
  arr_clear(a64_ex_args);
  arr_clear(a64_ex_acnt);
  return 0;
  return 0;
}

long long emit_module_arm64(long long mi) {
  long long fn_start, code_start, i, rec, name_p, idx, j;
  fn_start = arr_len(a64_fn_name);
  i = 0;
  while ((i < func_cnt)) {
    if ((arr_get(func_mods, i) == mi)) {
      rec = (*(long long*)((uintptr_t)((funcs + (i * 8)))));
      name_p = (*(long long*)((uintptr_t)((rec + FN_NAME))));
      idx = (-1);
      j = 0;
      while ((j < arr_len(a64_fn_name))) {
        if (a64_str_eq_packed(name_p, arr_get(a64_fn_name, j))) {
          idx = j;
        }
        j = (j + 1);
      }
      if ((idx < 0)) {
        idx = arr_len(a64_fn_name);
        a64_fn_name = arr_push(a64_fn_name, name_p);
        a64_fn_par = arr_push(a64_fn_par, (*(long long*)((uintptr_t)((rec + FN_PARS)))));
        a64_fn_pcnt = arr_push(a64_fn_pcnt, (*(long long*)((uintptr_t)((rec + FN_PCNT)))));
        a64_fn_body = arr_push(a64_fn_body, (*(long long*)((uintptr_t)((rec + FN_BODY)))));
        a64_fn_bcnt = arr_push(a64_fn_bcnt, (*(long long*)((uintptr_t)((rec + FN_BCNT)))));
        a64_fn_fidx = arr_push(a64_fn_fidx, i);
      } else {
        arr_set(a64_fn_name, idx, name_p);
        arr_set(a64_fn_par, idx, (*(long long*)((uintptr_t)((rec + FN_PARS)))));
        arr_set(a64_fn_pcnt, idx, (*(long long*)((uintptr_t)((rec + FN_PCNT)))));
        arr_set(a64_fn_body, idx, (*(long long*)((uintptr_t)((rec + FN_BODY)))));
        arr_set(a64_fn_bcnt, idx, (*(long long*)((uintptr_t)((rec + FN_BCNT)))));
        arr_set(a64_fn_fidx, idx, i);
      }
    }
    i = (i + 1);
  }
  i = fn_start;
  while ((i < arr_len(a64_fn_name))) {
    a64_fn_lbl = arr_push(a64_fn_lbl, enc_new_label());
    i = (i + 1);
  }
  arr_clear(a64_ex_name);
  arr_clear(a64_ex_ret);
  arr_clear(a64_ex_args);
  arr_clear(a64_ex_acnt);
  i = 0;
  while ((i < ext_cnt)) {
    rec = (*(long long*)((uintptr_t)((externs + (i * 8)))));
    a64_ex_name = arr_push(a64_ex_name, (*(long long*)((uintptr_t)((rec + EX_NAME)))));
    a64_ex_ret = arr_push(a64_ex_ret, (*(long long*)((uintptr_t)((rec + EX_RET)))));
    a64_ex_args = arr_push(a64_ex_args, (*(long long*)((uintptr_t)((rec + EX_ARGS)))));
    a64_ex_acnt = arr_push(a64_ex_acnt, (*(long long*)((uintptr_t)((rec + EX_ACNT)))));
    i = (i + 1);
  }
  enc_fix_cnt = 0;
  arr_clear(enc_fix_pos);
  arr_clear(enc_fix_lbl);
  arr_clear(enc_fix_ty);
  code_start = enc_pos;
  i = fn_start;
  while ((i < arr_len(a64_fn_name))) {
    a64_emit_func(i);
    i = (i + 1);
  }
  enc_resolve_fixups();
  return code_start;
  return 0;
}

uint8_t emit_all_arm64(void) {
  long long i, sp, ss, sl, j, ch, fp, fs, fl;
  bridge_data_arm64();
  if (a64_bin_mode) {
    a64_gl_name = arr_push(a64_gl_name, a64_argc_sym);
    a64_gl_name = arr_push(a64_gl_name, a64_argv_sym);
    enc_pos = 0;
    enc_lbl_max = 0;
    arr_clear(enc_lbl_off);
    enc_fix_cnt = 0;
    arr_clear(enc_fix_pos);
    arr_clear(enc_fix_lbl);
    arr_clear(enc_fix_ty);
    enc_rel_cnt = 0;
    arr_clear(enc_rel_pos);
    arr_clear(enc_rel_sym);
    arr_clear(enc_rel_ty);
    arr_clear(a64_fn_lbl);
    i = 0;
    while ((i < arr_len(a64_fn_name))) {
      a64_fn_lbl = arr_push(a64_fn_lbl, enc_new_label());
      i = (i + 1);
    }
    i = 0;
    while ((i < arr_len(a64_fn_name))) {
      a64_emit_func(i);
      i = (i + 1);
    }
    enc_resolve_fixups();
  } else {
    out((long long)".section __TEXT,__text,regular,pure_instructions");
    (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
    (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
    i = 0;
    while ((i < arr_len(a64_fn_name))) {
      a64_emit_func(i);
      i = (i + 1);
    }
    out((long long)".section __DATA,__data");
    (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
    out((long long)".p2align 3");
    (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
    out((long long)"_bpp_argc: .quad 0");
    (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
    out((long long)"_bpp_argv: .quad 0");
    (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
    i = 0;
    while ((i < arr_len(a64_gl_name))) {
      out((long long)".globl _");
      a64_print_p(arr_get(a64_gl_name, i));
      (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
      out((long long)".p2align 3");
      (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
      out((long long)"_");
      a64_print_p(arr_get(a64_gl_name, i));
      out((long long)":");
      (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
      out((long long)"  .quad 0");
      (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
      i = (i + 1);
    }
    (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
    if ((arr_len(a64_str_tbl) > 0)) {
      out((long long)".section __TEXT,__cstring,cstring_literals");
      (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
      i = 0;
      while ((i < arr_len(a64_str_tbl))) {
        sp = arr_get(a64_str_tbl, i);
        ss = unpack_s(sp);
        sl = unpack_l(sp);
        out((long long)"_str_");
        a64_print_dec(i);
        out((long long)":");
        (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
        out((long long)"  .asciz ");
        (_bpp_scratch[0] = (uint8_t)(34), bpp_sys_write(1, _bpp_scratch, 1));
        j = 0;
        while ((j < sl)) {
          ch = ((long long)(*(uint8_t*)((uintptr_t)(((a64_sbuf + ss) + j)))));
          if ((ch == 10)) {
            (_bpp_scratch[0] = (uint8_t)(92), bpp_sys_write(1, _bpp_scratch, 1));
            (_bpp_scratch[0] = (uint8_t)(110), bpp_sys_write(1, _bpp_scratch, 1));
          } else {
            if ((ch == 34)) {
              (_bpp_scratch[0] = (uint8_t)(92), bpp_sys_write(1, _bpp_scratch, 1));
              (_bpp_scratch[0] = (uint8_t)(34), bpp_sys_write(1, _bpp_scratch, 1));
            } else {
              if ((ch == 92)) {
                (_bpp_scratch[0] = (uint8_t)(92), bpp_sys_write(1, _bpp_scratch, 1));
                (_bpp_scratch[0] = (uint8_t)(92), bpp_sys_write(1, _bpp_scratch, 1));
              } else {
                (_bpp_scratch[0] = (uint8_t)(ch), bpp_sys_write(1, _bpp_scratch, 1));
              }
            }
          }
          j = (j + 1);
        }
        (_bpp_scratch[0] = (uint8_t)(34), bpp_sys_write(1, _bpp_scratch, 1));
        (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
        i = (i + 1);
      }
      (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
    }
    if ((arr_len(a64_flt_tbl) > 0)) {
      out((long long)".section __DATA,__const");
      (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
      i = 0;
      while ((i < arr_len(a64_flt_tbl))) {
        fp = arr_get(a64_flt_tbl, i);
        fs = unpack_s(fp);
        fl = unpack_l(fp);
        out((long long)".p2align 3");
        (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
        out((long long)"_flt_");
        a64_print_dec(i);
        out((long long)":");
        (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
        out((long long)"  .double ");
        j = 0;
        while ((j < fl)) {
          (_bpp_scratch[0] = (uint8_t)(((long long)(*(uint8_t*)((uintptr_t)(((a64_sbuf + fs) + j)))))), bpp_sys_write(1, _bpp_scratch, 1));
          j = (j + 1);
        }
        (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
        i = (i + 1);
      }
      (_bpp_scratch[0] = (uint8_t)(10), bpp_sys_write(1, _bpp_scratch, 1));
    }
  }
  return 0;
  return 0;
}

long long u32(long long x) {
  return (x & 0xFFFFFFFF);
  return 0;
}

long long sha_rotr(long long x, long long n) {
  return u32(((x >> n) | ((x & 0xFFFFFFFF) << (32 - n))));
  return 0;
}

uint8_t sha_init(void) {
  sha_k = (long long)malloc(512);
  sha_w = (long long)malloc(512);
  *(long long*)((uintptr_t)((sha_k + 0))) = 0x428a2f98;
  *(long long*)((uintptr_t)((sha_k + 8))) = 0x71374491;
  *(long long*)((uintptr_t)((sha_k + 16))) = 0xb5c0fbcf;
  *(long long*)((uintptr_t)((sha_k + 24))) = 0xe9b5dba5;
  *(long long*)((uintptr_t)((sha_k + 32))) = 0x3956c25b;
  *(long long*)((uintptr_t)((sha_k + 40))) = 0x59f111f1;
  *(long long*)((uintptr_t)((sha_k + 48))) = 0x923f82a4;
  *(long long*)((uintptr_t)((sha_k + 56))) = 0xab1c5ed5;
  *(long long*)((uintptr_t)((sha_k + 64))) = 0xd807aa98;
  *(long long*)((uintptr_t)((sha_k + 72))) = 0x12835b01;
  *(long long*)((uintptr_t)((sha_k + 80))) = 0x243185be;
  *(long long*)((uintptr_t)((sha_k + 88))) = 0x550c7dc3;
  *(long long*)((uintptr_t)((sha_k + 96))) = 0x72be5d74;
  *(long long*)((uintptr_t)((sha_k + 104))) = 0x80deb1fe;
  *(long long*)((uintptr_t)((sha_k + 112))) = 0x9bdc06a7;
  *(long long*)((uintptr_t)((sha_k + 120))) = 0xc19bf174;
  *(long long*)((uintptr_t)((sha_k + 128))) = 0xe49b69c1;
  *(long long*)((uintptr_t)((sha_k + 136))) = 0xefbe4786;
  *(long long*)((uintptr_t)((sha_k + 144))) = 0x0fc19dc6;
  *(long long*)((uintptr_t)((sha_k + 152))) = 0x240ca1cc;
  *(long long*)((uintptr_t)((sha_k + 160))) = 0x2de92c6f;
  *(long long*)((uintptr_t)((sha_k + 168))) = 0x4a7484aa;
  *(long long*)((uintptr_t)((sha_k + 176))) = 0x5cb0a9dc;
  *(long long*)((uintptr_t)((sha_k + 184))) = 0x76f988da;
  *(long long*)((uintptr_t)((sha_k + 192))) = 0x983e5152;
  *(long long*)((uintptr_t)((sha_k + 200))) = 0xa831c66d;
  *(long long*)((uintptr_t)((sha_k + 208))) = 0xb00327c8;
  *(long long*)((uintptr_t)((sha_k + 216))) = 0xbf597fc7;
  *(long long*)((uintptr_t)((sha_k + 224))) = 0xc6e00bf3;
  *(long long*)((uintptr_t)((sha_k + 232))) = 0xd5a79147;
  *(long long*)((uintptr_t)((sha_k + 240))) = 0x06ca6351;
  *(long long*)((uintptr_t)((sha_k + 248))) = 0x14292967;
  *(long long*)((uintptr_t)((sha_k + 256))) = 0x27b70a85;
  *(long long*)((uintptr_t)((sha_k + 264))) = 0x2e1b2138;
  *(long long*)((uintptr_t)((sha_k + 272))) = 0x4d2c6dfc;
  *(long long*)((uintptr_t)((sha_k + 280))) = 0x53380d13;
  *(long long*)((uintptr_t)((sha_k + 288))) = 0x650a7354;
  *(long long*)((uintptr_t)((sha_k + 296))) = 0x766a0abb;
  *(long long*)((uintptr_t)((sha_k + 304))) = 0x81c2c92e;
  *(long long*)((uintptr_t)((sha_k + 312))) = 0x92722c85;
  *(long long*)((uintptr_t)((sha_k + 320))) = 0xa2bfe8a1;
  *(long long*)((uintptr_t)((sha_k + 328))) = 0xa81a664b;
  *(long long*)((uintptr_t)((sha_k + 336))) = 0xc24b8b70;
  *(long long*)((uintptr_t)((sha_k + 344))) = 0xc76c51a3;
  *(long long*)((uintptr_t)((sha_k + 352))) = 0xd192e819;
  *(long long*)((uintptr_t)((sha_k + 360))) = 0xd6990624;
  *(long long*)((uintptr_t)((sha_k + 368))) = 0xf40e3585;
  *(long long*)((uintptr_t)((sha_k + 376))) = 0x106aa070;
  *(long long*)((uintptr_t)((sha_k + 384))) = 0x19a4c116;
  *(long long*)((uintptr_t)((sha_k + 392))) = 0x1e376c08;
  *(long long*)((uintptr_t)((sha_k + 400))) = 0x2748774c;
  *(long long*)((uintptr_t)((sha_k + 408))) = 0x34b0bcb5;
  *(long long*)((uintptr_t)((sha_k + 416))) = 0x391c0cb3;
  *(long long*)((uintptr_t)((sha_k + 424))) = 0x4ed8aa4a;
  *(long long*)((uintptr_t)((sha_k + 432))) = 0x5b9cca4f;
  *(long long*)((uintptr_t)((sha_k + 440))) = 0x682e6ff3;
  *(long long*)((uintptr_t)((sha_k + 448))) = 0x748f82ee;
  *(long long*)((uintptr_t)((sha_k + 456))) = 0x78a5636f;
  *(long long*)((uintptr_t)((sha_k + 464))) = 0x84c87814;
  *(long long*)((uintptr_t)((sha_k + 472))) = 0x8cc70208;
  *(long long*)((uintptr_t)((sha_k + 480))) = 0x90befffa;
  *(long long*)((uintptr_t)((sha_k + 488))) = 0xa4506ceb;
  *(long long*)((uintptr_t)((sha_k + 496))) = 0xbef9a3f7;
  *(long long*)((uintptr_t)((sha_k + 504))) = 0xc67178f2;
  return 0;
  return 0;
}

uint8_t sha_transform(long long blk, long long h) {
  long long i, s0, s1, ch, maj, t1, t2;
  long long a, b, c, d, e, f, g, hh;
  i = 0;
  while ((i < 16)) {
    *(long long*)((uintptr_t)((sha_w + (i * 8)))) = ((((((long long)(*(uint8_t*)((uintptr_t)((blk + (i * 4)))))) << 24) | (((long long)(*(uint8_t*)((uintptr_t)(((blk + (i * 4)) + 1))))) << 16)) | (((long long)(*(uint8_t*)((uintptr_t)(((blk + (i * 4)) + 2))))) << 8)) | ((long long)(*(uint8_t*)((uintptr_t)(((blk + (i * 4)) + 3))))));
    i = (i + 1);
  }
  while ((i < 64)) {
    s0 = ((sha_rotr((*(long long*)((uintptr_t)((sha_w + ((i - 15) * 8))))), 7) ^ sha_rotr((*(long long*)((uintptr_t)((sha_w + ((i - 15) * 8))))), 18)) ^ ((*(long long*)((uintptr_t)((sha_w + ((i - 15) * 8))))) >> 3));
    s1 = ((sha_rotr((*(long long*)((uintptr_t)((sha_w + ((i - 2) * 8))))), 17) ^ sha_rotr((*(long long*)((uintptr_t)((sha_w + ((i - 2) * 8))))), 19)) ^ ((*(long long*)((uintptr_t)((sha_w + ((i - 2) * 8))))) >> 10));
    *(long long*)((uintptr_t)((sha_w + (i * 8)))) = u32(((((*(long long*)((uintptr_t)((sha_w + ((i - 16) * 8))))) + s0) + (*(long long*)((uintptr_t)((sha_w + ((i - 7) * 8)))))) + s1));
    i = (i + 1);
  }
  a = (*(long long*)((uintptr_t)((h + 0))));
  b = (*(long long*)((uintptr_t)((h + 8))));
  c = (*(long long*)((uintptr_t)((h + 16))));
  d = (*(long long*)((uintptr_t)((h + 24))));
  e = (*(long long*)((uintptr_t)((h + 32))));
  f = (*(long long*)((uintptr_t)((h + 40))));
  g = (*(long long*)((uintptr_t)((h + 48))));
  hh = (*(long long*)((uintptr_t)((h + 56))));
  i = 0;
  while ((i < 64)) {
    s1 = ((sha_rotr(e, 6) ^ sha_rotr(e, 11)) ^ sha_rotr(e, 25));
    ch = ((e & f) ^ ((e ^ 0xFFFFFFFF) & g));
    t1 = u32(((((hh + s1) + ch) + (*(long long*)((uintptr_t)((sha_k + (i * 8)))))) + (*(long long*)((uintptr_t)((sha_w + (i * 8)))))));
    s0 = ((sha_rotr(a, 2) ^ sha_rotr(a, 13)) ^ sha_rotr(a, 22));
    maj = (((a & b) ^ (a & c)) ^ (b & c));
    t2 = u32((s0 + maj));
    hh = g;
    g = f;
    f = e;
    e = u32((d + t1));
    d = c;
    c = b;
    b = a;
    a = u32((t1 + t2));
    i = (i + 1);
  }
  *(long long*)((uintptr_t)((h + 0))) = u32(((*(long long*)((uintptr_t)((h + 0)))) + a));
  *(long long*)((uintptr_t)((h + 8))) = u32(((*(long long*)((uintptr_t)((h + 8)))) + b));
  *(long long*)((uintptr_t)((h + 16))) = u32(((*(long long*)((uintptr_t)((h + 16)))) + c));
  *(long long*)((uintptr_t)((h + 24))) = u32(((*(long long*)((uintptr_t)((h + 24)))) + d));
  *(long long*)((uintptr_t)((h + 32))) = u32(((*(long long*)((uintptr_t)((h + 32)))) + e));
  *(long long*)((uintptr_t)((h + 40))) = u32(((*(long long*)((uintptr_t)((h + 40)))) + f));
  *(long long*)((uintptr_t)((h + 48))) = u32(((*(long long*)((uintptr_t)((h + 48)))) + g));
  *(long long*)((uintptr_t)((h + 56))) = u32(((*(long long*)((uintptr_t)((h + 56)))) + hh));
  return 0;
  return 0;
}

uint8_t sha256(long long data, long long len, long long out) {
  long long h, pad, padlen, bitlen, total, off, i, v;
  h = (long long)malloc(64);
  *(long long*)((uintptr_t)((h + 0))) = 0x6a09e667;
  *(long long*)((uintptr_t)((h + 8))) = 0xbb67ae85;
  *(long long*)((uintptr_t)((h + 16))) = 0x3c6ef372;
  *(long long*)((uintptr_t)((h + 24))) = 0xa54ff53a;
  *(long long*)((uintptr_t)((h + 32))) = 0x510e527f;
  *(long long*)((uintptr_t)((h + 40))) = 0x9b05688c;
  *(long long*)((uintptr_t)((h + 48))) = 0x1f83d9ab;
  *(long long*)((uintptr_t)((h + 56))) = 0x5be0cd19;
  bitlen = (len * 8);
  padlen = (len + 1);
  while (((padlen % 64) != 56)) {
    padlen = (padlen + 1);
  }
  total = (padlen + 8);
  pad = (long long)malloc(total);
  i = 0;
  while ((i < len)) {
    (*(uint8_t*)((uintptr_t)((pad + i))) = (uint8_t)(((long long)(*(uint8_t*)((uintptr_t)((data + i)))))));
    i = (i + 1);
  }
  (*(uint8_t*)((uintptr_t)((pad + len))) = (uint8_t)(0x80));
  i = (len + 1);
  while ((i < padlen)) {
    (*(uint8_t*)((uintptr_t)((pad + i))) = (uint8_t)(0));
    i = (i + 1);
  }
  (*(uint8_t*)((uintptr_t)((pad + padlen))) = (uint8_t)(((bitlen >> 56) & 0xFF)));
  (*(uint8_t*)((uintptr_t)(((pad + padlen) + 1))) = (uint8_t)(((bitlen >> 48) & 0xFF)));
  (*(uint8_t*)((uintptr_t)(((pad + padlen) + 2))) = (uint8_t)(((bitlen >> 40) & 0xFF)));
  (*(uint8_t*)((uintptr_t)(((pad + padlen) + 3))) = (uint8_t)(((bitlen >> 32) & 0xFF)));
  (*(uint8_t*)((uintptr_t)(((pad + padlen) + 4))) = (uint8_t)(((bitlen >> 24) & 0xFF)));
  (*(uint8_t*)((uintptr_t)(((pad + padlen) + 5))) = (uint8_t)(((bitlen >> 16) & 0xFF)));
  (*(uint8_t*)((uintptr_t)(((pad + padlen) + 6))) = (uint8_t)(((bitlen >> 8) & 0xFF)));
  (*(uint8_t*)((uintptr_t)(((pad + padlen) + 7))) = (uint8_t)((bitlen & 0xFF)));
  off = 0;
  while ((off < total)) {
    sha_transform((pad + off), h);
    off = (off + 64);
  }
  i = 0;
  while ((i < 8)) {
    v = (*(long long*)((uintptr_t)((h + (i * 8)))));
    (*(uint8_t*)((uintptr_t)((out + (i * 4)))) = (uint8_t)(((v >> 24) & 0xFF)));
    (*(uint8_t*)((uintptr_t)(((out + (i * 4)) + 1))) = (uint8_t)(((v >> 16) & 0xFF)));
    (*(uint8_t*)((uintptr_t)(((out + (i * 4)) + 2))) = (uint8_t)(((v >> 8) & 0xFF)));
    (*(uint8_t*)((uintptr_t)(((out + (i * 4)) + 3))) = (uint8_t)((v & 0xFF)));
    i = (i + 1);
  }
  (free((void*)(uintptr_t)(h)), 0);
  (free((void*)(uintptr_t)(pad)), 0);
  return 0;
  return 0;
}

uint8_t init_macho(void) {
  sha_init();
  MO_PAGE_SIZE = 16384;
  MO_VM_BASE = 0;
  MO_VM_BASE_HI = 1;
  mo_buf = (long long)malloc(16777216);
  mo_pos = 0;
  mo_flt_data = arr_new();
  mo_sdata = (long long)malloc(1048576);
  mo_sdata_size = 0;
  mo_sdata_off = arr_new();
  mo_got_syms = arr_new();
  mo_got_libs = arr_new();
  mo_ext_libs = arr_new();
  return 0;
  return 0;
}

uint8_t mo_packed_eq(long long a, long long b) {
  long long as, al, bs, bl, i;
  if ((a == b)) {
    return 1;
  }
  if ((a < 0)) {
    return 0;
  }
  if ((b < 0)) {
    return 0;
  }
  al = (a / 0x100000000);
  bl = (b / 0x100000000);
  if ((al != bl)) {
    return 0;
  }
  as = (a % 0x100000000);
  bs = (b % 0x100000000);
  i = 0;
  while ((i < al)) {
    if ((((long long)(*(uint8_t*)((uintptr_t)(((a64_sbuf + as) + i))))) != ((long long)(*(uint8_t*)((uintptr_t)(((a64_sbuf + bs) + i))))))) {
      return 0;
    }
    i = (i + 1);
  }
  return 1;
  return 0;
}

uint16_t mo_atof(long long s) {
  long long ch, i, neg, int_part, frac_val, frac_scale;
  long long num, den, p, scaled_den, mantissa, bit, exp_bits;
  neg = 0;
  i = 0;
  ch = ((long long)(*(uint8_t*)((uintptr_t)(s))));
  if ((ch == 45)) {
    neg = 1;
    i = (i + 1);
  }
  int_part = 0;
  ch = ((long long)(*(uint8_t*)((uintptr_t)((s + i)))));
  while ((ch >= 48)) {
    if ((ch <= 57)) {
      int_part = (((int_part * 10) + ch) - 48);
      i = (i + 1);
      ch = ((long long)(*(uint8_t*)((uintptr_t)((s + i)))));
    } else {
      ch = 0;
    }
  }
  frac_val = 0;
  frac_scale = 1;
  if ((((long long)(*(uint8_t*)((uintptr_t)((s + i))))) == 46)) {
    i = (i + 1);
    ch = ((long long)(*(uint8_t*)((uintptr_t)((s + i)))));
    while ((ch >= 48)) {
      if ((ch <= 57)) {
        frac_val = (((frac_val * 10) + ch) - 48);
        frac_scale = (frac_scale * 10);
        i = (i + 1);
        ch = ((long long)(*(uint8_t*)((uintptr_t)((s + i)))));
      } else {
        ch = 0;
      }
    }
  }
  num = ((int_part * frac_scale) + frac_val);
  den = frac_scale;
  if ((num == 0)) {
    return 0;
  }
  if ((num >= den)) {
    scaled_den = den;
    p = 0;
    while (((scaled_den + scaled_den) <= num)) {
      scaled_den = (scaled_den + scaled_den);
      p = (p + 1);
    }
    num = (num - scaled_den);
  } else {
    p = (0 - 1);
    num = (num + num);
    while ((num < den)) {
      num = (num + num);
      p = (p - 1);
    }
    num = (num - den);
    scaled_den = den;
  }
  mantissa = 0;
  bit = 0;
  while ((bit < 52)) {
    num = (num + num);
    mantissa = (mantissa + mantissa);
    if ((num >= scaled_den)) {
      mantissa = (mantissa + 1);
      num = (num - scaled_den);
    }
    bit = (bit + 1);
  }
  num = (num + num);
  if ((num >= scaled_den)) {
    mantissa = (mantissa + 1);
  }
  exp_bits = (p + 1023);
  if (neg) {
    exp_bits = (exp_bits + 2048);
  }
  return ((exp_bits << 52) + mantissa);
  return 0;
}

uint8_t mo_add_float(long long str_ptr) {
  long long bits;
  bits = mo_atof(str_ptr);
  mo_flt_data = arr_push(mo_flt_data, bits);
  return 0;
  return 0;
}

uint8_t mo_add_string(long long src, long long len) {
  long long i;
  mo_sdata_off = arr_push(mo_sdata_off, mo_sdata_size);
  i = 0;
  while ((i < len)) {
    (*(uint8_t*)((uintptr_t)((mo_sdata + mo_sdata_size))) = (uint8_t)(((long long)(*(uint8_t*)((uintptr_t)((src + i)))))));
    mo_sdata_size = (mo_sdata_size + 1);
    i = (i + 1);
  }
  (*(uint8_t*)((uintptr_t)((mo_sdata + mo_sdata_size))) = (uint8_t)(0));
  mo_sdata_size = (mo_sdata_size + 1);
  return 0;
  return 0;
}

uint8_t mo_is_system_lib(long long lib_name) {
  long long ss, sl;
  ss = unpack_s(lib_name);
  sl = unpack_l(lib_name);
  if ((sl == 6)) {
    if ((((long long)(*(uint8_t*)((uintptr_t)((a64_sbuf + ss))))) == 83)) {
      if ((((long long)(*(uint8_t*)((uintptr_t)(((a64_sbuf + ss) + 1))))) == 121)) {
        if ((((long long)(*(uint8_t*)((uintptr_t)(((a64_sbuf + ss) + 2))))) == 115)) {
          return 1;
        }
      }
    }
  }
  return 0;
  return 0;
}

uint8_t mo_add_got(long long fn_name, long long lib_name) {
  long long i, found;
  mo_got_syms = arr_push(mo_got_syms, fn_name);
  mo_got_libs = arr_push(mo_got_libs, lib_name);
  if (mo_is_system_lib(lib_name)) {
    return 0;
  }
  found = 0;
  i = 0;
  while ((i < arr_len(mo_ext_libs))) {
    if (mo_packed_eq(arr_get(mo_ext_libs, i), lib_name)) {
      found = 1;
    }
    i = (i + 1);
  }
  if ((found == 0)) {
    mo_ext_libs = arr_push(mo_ext_libs, lib_name);
  }
  return 0;
  return 0;
}

uint8_t mo_find_got(long long fn_name) {
  long long i;
  i = 0;
  while ((i < arr_len(mo_got_syms))) {
    if (mo_packed_eq(arr_get(mo_got_syms, i), fn_name)) {
      return i;
    }
    i = (i + 1);
  }
  return (-1);
  return 0;
}

uint8_t mo_find_lib_ordinal(long long lib_name) {
  long long i;
  if (mo_is_system_lib(lib_name)) {
    return 1;
  }
  i = 0;
  while ((i < arr_len(mo_ext_libs))) {
    if (mo_packed_eq(arr_get(mo_ext_libs, i), lib_name)) {
      return (i + 2);
    }
    i = (i + 1);
  }
  return 1;
  return 0;
}

uint8_t mo_w8(long long v) {
  (*(uint8_t*)((uintptr_t)((mo_buf + mo_pos))) = (uint8_t)((v & 0xFF)));
  mo_pos = (mo_pos + 1);
  return 0;
  return 0;
}

uint8_t mo_w32(long long v) {
  (*(uint8_t*)((uintptr_t)((mo_buf + mo_pos))) = (uint8_t)((v & 0xFF)));
  (*(uint8_t*)((uintptr_t)(((mo_buf + mo_pos) + 1))) = (uint8_t)(((v >> 8) & 0xFF)));
  (*(uint8_t*)((uintptr_t)(((mo_buf + mo_pos) + 2))) = (uint8_t)(((v >> 16) & 0xFF)));
  (*(uint8_t*)((uintptr_t)(((mo_buf + mo_pos) + 3))) = (uint8_t)(((v >> 24) & 0xFF)));
  mo_pos = (mo_pos + 4);
  return 0;
  return 0;
}

uint8_t mo_w64(long long lo, long long hi) {
  mo_w32(lo);
  mo_w32(hi);
  return 0;
  return 0;
}

uint8_t mo_pad(long long n) {
  long long i;
  i = 0;
  while ((i < n)) {
    (*(uint8_t*)((uintptr_t)((mo_buf + mo_pos))) = (uint8_t)(0));
    mo_pos = (mo_pos + 1);
    i = (i + 1);
  }
  return 0;
  return 0;
}

uint8_t mo_align(long long a) {
  while (((mo_pos % a) != 0)) {
    (*(uint8_t*)((uintptr_t)((mo_buf + mo_pos))) = (uint8_t)(0));
    mo_pos = (mo_pos + 1);
  }
  return 0;
  return 0;
}

uint8_t mo_wstr(long long s, long long slen, long long n) {
  long long i;
  i = 0;
  while ((i < n)) {
    if ((i < slen)) {
      (*(uint8_t*)((uintptr_t)((mo_buf + mo_pos))) = (uint8_t)(((long long)(*(uint8_t*)((uintptr_t)((s + i)))))));
    } else {
      (*(uint8_t*)((uintptr_t)((mo_buf + mo_pos))) = (uint8_t)(0));
    }
    mo_pos = (mo_pos + 1);
    i = (i + 1);
  }
  return 0;
  return 0;
}

uint8_t mo_wcstr(long long s, long long n) {
  long long i;
  i = 0;
  while ((i < n)) {
    (*(uint8_t*)((uintptr_t)((mo_buf + mo_pos))) = (uint8_t)((long long)((uint8_t*)((uintptr_t)(s)))[(i)]));
    mo_pos = (mo_pos + 1);
    i = (i + 1);
  }
  return 0;
  return 0;
}

long long mo_page_align(long long v) {
  return ((((v + MO_PAGE_SIZE) - 1) / MO_PAGE_SIZE) * MO_PAGE_SIZE);
  return 0;
}

uint8_t mo_write_header(void) {
  mo_w32(0xFEEDFACF);
  mo_w32(0x0100000C);
  mo_w32(0x00000000);
  mo_w32(0x00000002);
  mo_w32(mo_ncmds);
  mo_w32(mo_sizeofcmds);
  mo_w32(0x00200085);
  mo_w32(0x00000000);
  return 0;
  return 0;
}

uint8_t mo_write_segment(long long name, long long nlen, long long vmaddr_lo, long long vmaddr_hi, long long vmsize, long long fileoff, long long filesize, long long maxprot, long long initprot, long long nsects) {
  mo_w32(0x19);
  mo_w32((72 + (nsects * 80)));
  mo_wcstr(name, nlen);
  mo_pad((16 - nlen));
  mo_w64(vmaddr_lo, vmaddr_hi);
  mo_w64(vmsize, 0);
  mo_w64(fileoff, 0);
  mo_w64(filesize, 0);
  mo_w32(maxprot);
  mo_w32(initprot);
  mo_w32(nsects);
  mo_w32(0);
  return 0;
  return 0;
}

uint8_t mo_write_section(long long sectname, long long snlen, long long segname, long long sgnlen, long long addr_lo, long long addr_hi, long long size, long long offset, long long al, long long flags) {
  mo_wcstr(sectname, snlen);
  mo_pad((16 - snlen));
  mo_wcstr(segname, sgnlen);
  mo_pad((16 - sgnlen));
  mo_w64(addr_lo, addr_hi);
  mo_w64(size, 0);
  mo_w32(offset);
  mo_w32(al);
  mo_w32(0);
  mo_w32(0);
  mo_w32(flags);
  mo_w32(0);
  mo_w32(0);
  mo_w32(0);
  return 0;
  return 0;
}

uint8_t mo_write_lc_chained_fixups(void) {
  mo_w32(0x80000034);
  mo_w32(16);
  mo_w32(mo_cf_off);
  mo_w32(mo_cf_size);
  return 0;
  return 0;
}

uint8_t mo_write_lc_exports_trie(void) {
  mo_w32(0x80000033);
  mo_w32(16);
  mo_w32(mo_et_off);
  mo_w32(mo_et_size);
  return 0;
  return 0;
}

uint8_t mo_write_lc_symtab(void) {
  mo_w32(0x02);
  mo_w32(24);
  mo_w32(mo_sym_off);
  mo_w32(mo_sym_cnt);
  mo_w32(mo_str_off);
  mo_w32(mo_str_size);
  return 0;
  return 0;
}

uint8_t mo_write_lc_dysymtab(void) {
  mo_w32(0x0B);
  mo_w32(80);
  mo_w32(0);
  mo_w32(0);
  mo_w32(0);
  mo_w32(mo_sym_cnt);
  mo_w32(mo_sym_cnt);
  mo_w32(0);
  mo_pad(48);
  return 0;
  return 0;
}

uint8_t mo_write_lc_dylinker(void) {
  mo_w32(0x0E);
  mo_w32(32);
  mo_w32(12);
  mo_wcstr((long long)"/usr/lib/dyld", 13);
  mo_pad(7);
  return 0;
  return 0;
}

uint8_t mo_write_lc_main(void) {
  mo_w32(0x80000028);
  mo_w32(24);
  mo_w64(mo_main_off, 0);
  mo_w64(0, 0);
  return 0;
  return 0;
}

uint8_t mo_write_lc_load_dylib(void) {
  mo_w32(0x0C);
  mo_w32(56);
  mo_w32(24);
  mo_w32(2);
  mo_w32(0x05470000);
  mo_w32(0x00010000);
  mo_wcstr((long long)"/usr/lib/libSystem.B.dylib", 26);
  mo_pad(6);
  return 0;
  return 0;
}

uint8_t mo_write_chained_fixups(long long nseg) {
  long long i;
  mo_w32(0);
  mo_w32(32);
  mo_w32(((32 + 4) + (nseg * 4)));
  mo_w32(((32 + 4) + (nseg * 4)));
  mo_w32(0);
  mo_w32(1);
  mo_w32(0);
  mo_pad(4);
  mo_w32(nseg);
  i = 0;
  while ((i < nseg)) {
    mo_w32(0);
    i = (i + 1);
  }
  mo_pad(8);
  return 0;
  return 0;
}

uint8_t mo_write_chained_fixups_real(long long nseg, long long gl_count) {
  long long i, start, data_seg_idx;
  long long starts_off, starts_in_seg_off, imports_off, symbols_off;
  long long got_off_in_seg, got_page, got_page_off;
  long long lib_ord, name_off, sym_pos, ss, sl, j, imp_val;
  start = mo_pos;
  data_seg_idx = (nseg - 2);
  got_off_in_seg = mo_got_data_off;
  got_page = (got_off_in_seg / MO_PAGE_SIZE);
  got_page_off = (got_off_in_seg % MO_PAGE_SIZE);
  starts_off = 32;
  starts_in_seg_off = ((starts_off + 4) + (data_seg_idx * 4));
  imports_off = (((starts_off + 4) + (nseg * 4)) + 24);
  symbols_off = (imports_off + (arr_len(mo_got_syms) * 4));
  mo_w32(0);
  mo_w32(starts_off);
  mo_w32(imports_off);
  mo_w32(symbols_off);
  mo_w32(arr_len(mo_got_syms));
  mo_w32(1);
  mo_w32(0);
  mo_pad(4);
  mo_w32(nseg);
  i = 0;
  while ((i < nseg)) {
    if ((i == data_seg_idx)) {
      mo_w32((4 + (nseg * 4)));
    } else {
      mo_w32(0);
    }
    i = (i + 1);
  }
  mo_w32(24);
  mo_w8(0);
  mo_w8(0x40);
  mo_w8(6);
  mo_w8(0);
  mo_w64(mo_text_vmsize, 0);
  mo_w32(0);
  mo_w8(1);
  mo_w8(0);
  mo_w8((got_page_off & 0xFF));
  mo_w8(((got_page_off >> 8) & 0xFF));
  sym_pos = 1;
  i = 0;
  while ((i < arr_len(mo_got_syms))) {
    lib_ord = mo_find_lib_ordinal(arr_get(mo_got_libs, i));
    imp_val = ((lib_ord & 0xFF) | ((sym_pos & 0x7FFFFF) << 9));
    mo_w32(imp_val);
    sl = unpack_l(arr_get(mo_got_syms, i));
    sym_pos = (((sym_pos + 1) + sl) + 1);
    i = (i + 1);
  }
  mo_w8(0);
  i = 0;
  while ((i < arr_len(mo_got_syms))) {
    ss = unpack_s(arr_get(mo_got_syms, i));
    sl = unpack_l(arr_get(mo_got_syms, i));
    mo_w8(0x5F);
    j = 0;
    while ((j < sl)) {
      mo_w8(((long long)(*(uint8_t*)((uintptr_t)(((a64_sbuf + ss) + j))))));
      j = (j + 1);
    }
    mo_w8(0);
    i = (i + 1);
  }
  while ((((mo_pos - start) % 4) != 0)) {
    mo_w8(0);
  }
  mo_cf_size = (mo_pos - start);
  return 0;
  return 0;
}

uint8_t mo_write_lc_load_dylib_named(long long lib_name) {
  long long ss, sl, i, pathlen;
  ss = unpack_s(lib_name);
  sl = unpack_l(lib_name);
  pathlen = (27 + sl);
  mo_w32(0x0C);
  mo_w32(80);
  mo_w32(24);
  mo_w32(2);
  mo_w32(0x00010000);
  mo_w32(0x00010000);
  mo_wcstr((long long)"/opt/homebrew/lib/lib", 21);
  i = 0;
  while ((i < sl)) {
    mo_w8(((long long)(*(uint8_t*)((uintptr_t)(((a64_sbuf + ss) + i))))));
    i = (i + 1);
  }
  mo_wcstr((long long)".dylib", 6);
  mo_w8(0);
  mo_pad((((80 - 24) - pathlen) - 1));
  return 0;
  return 0;
}

long long mo_write_exports_trie(long long main_file_offset) {
  long long start, lo, hi;
  start = mo_pos;
  mo_w8(0);
  mo_w8(1);
  mo_wcstr((long long)"_main", 5);
  mo_w8(0);
  mo_w8(9);
  mo_w8(3);
  mo_w8(0);
  lo = (main_file_offset & 0x7F);
  hi = ((main_file_offset >> 7) & 0x7F);
  if ((hi > 0)) {
    mo_w8((lo | 0x80));
    mo_w8(hi);
  } else {
    mo_w8(lo);
  }
  mo_w8(0);
  while ((((mo_pos - start) % 4) != 0)) {
    mo_w8(0);
  }
  return (mo_pos - start);
  return 0;
}

uint8_t mo_write_nlist(long long n_strx, long long n_type, long long n_sect, long long n_desc, long long n_value_lo, long long n_value_hi) {
  mo_w32(n_strx);
  mo_w8(n_type);
  mo_w8(n_sect);
  mo_w8((n_desc & 0xFF));
  mo_w8(((n_desc >> 8) & 0xFF));
  mo_w64(n_value_lo, n_value_hi);
  return 0;
  return 0;
}

uint8_t mo_compute_layout(long long code_size, long long data_count) {
  long long has_data, nseg, cf_data_size;
  has_data = 0;
  if ((data_count > 0)) {
    has_data = 1;
  }
  if ((arr_len(mo_flt_data) > 0)) {
    has_data = 1;
  }
  if ((mo_sdata_size > 0)) {
    has_data = 1;
  }
  if ((arr_len(mo_got_syms) > 0)) {
    has_data = 1;
  }
  nseg = 3;
  if (has_data) {
    nseg = 4;
  }
  mo_ncmds = ((11 + has_data) + arr_len(mo_ext_libs));
  mo_sizeofcmds = ((((((((((((72 + 152) + (has_data * 152)) + 72) + 16) + 16) + 24) + 80) + 32) + 24) + 56) + (arr_len(mo_ext_libs) * 80)) + 16);
  mo_code_off = ((32 + mo_sizeofcmds) + 16);
  mo_text_vmsize = mo_page_align((mo_code_off + code_size));
  mo_main_off = mo_code_off;
  if (has_data) {
    mo_data_off = mo_text_vmsize;
    mo_got_data_off = (((data_count * 8) + (arr_len(mo_flt_data) * 8)) + mo_sdata_size);
    mo_got_data_off = (((mo_got_data_off + 7) / 8) * 8);
    mo_data_size = (mo_got_data_off + (arr_len(mo_got_syms) * 8));
    mo_data_vmsize = mo_page_align(mo_data_size);
    mo_data_vmaddr_lo = (MO_VM_BASE + mo_text_vmsize);
    mo_data_vmaddr_hi = MO_VM_BASE_HI;
    if ((mo_data_vmaddr_lo < mo_text_vmsize)) {
      mo_data_vmaddr_hi = (mo_data_vmaddr_hi + 1);
    }
  } else {
    mo_data_off = mo_text_vmsize;
    mo_data_size = 0;
    mo_data_vmsize = 0;
    mo_data_vmaddr_lo = 0;
    mo_data_vmaddr_hi = 0;
  }
  mo_link_off = (mo_text_vmsize + mo_data_vmsize);
  mo_link_vmaddr_lo = (MO_VM_BASE + mo_link_off);
  mo_link_vmaddr_hi = MO_VM_BASE_HI;
  if ((mo_link_vmaddr_lo < mo_link_off)) {
    mo_link_vmaddr_hi = (mo_link_vmaddr_hi + 1);
  }
  if ((arr_len(mo_got_syms) > 0)) {
    long long sym_str_size, si;
    sym_str_size = 1;
    si = 0;
    while ((si < arr_len(mo_got_syms))) {
      sym_str_size = (((sym_str_size + 1) + unpack_l(arr_get(mo_got_syms, si))) + 1);
      si = (si + 1);
    }
    cf_data_size = (((((32 + 4) + (nseg * 4)) + 24) + (arr_len(mo_got_syms) * 4)) + sym_str_size);
    cf_data_size = (((cf_data_size + 3) / 4) * 4);
  } else {
    cf_data_size = (((32 + 4) + (nseg * 4)) + 8);
  }
  mo_cf_off = mo_link_off;
  mo_cf_size = cf_data_size;
  mo_et_off = (mo_cf_off + mo_cf_size);
  mo_et_size = 16;
  mo_sym_cnt = 2;
  mo_sym_off = (mo_et_off + mo_et_size);
  mo_str_off = (mo_sym_off + (mo_sym_cnt * 16));
  mo_str_size = 32;
  long long linkedit_content, n_code_slots, id_len;
  linkedit_content = (((mo_cf_size + mo_et_size) + (mo_sym_cnt * 16)) + mo_str_size);
  mo_cs_off = (mo_link_off + linkedit_content);
  mo_cs_off = (((mo_cs_off + 15) / 16) * 16);
  id_len = 6;
  n_code_slots = ((mo_cs_off + 4095) / 4096);
  mo_cs_size = ((((20 + 88) + id_len) + (2 * 32)) + (n_code_slots * 32));
  mo_link_size = ((mo_cs_off - mo_link_off) + mo_cs_size);
  mo_link_vmsize = mo_page_align(mo_link_size);
  return 0;
  return 0;
}

uint8_t mo_resolve_relocations(long long gl_names, long long gl_count) {
  long long i, pos, sym, ty, pc_addr, target_addr;
  long long page_diff, immlo, immhi, instr, imm12;
  long long gl_idx, j;
  i = 0;
  while ((i < enc_rel_cnt)) {
    pos = arr_get(enc_rel_pos, i);
    sym = arr_get(enc_rel_sym, i);
    ty = arr_get(enc_rel_ty, i);
    pc_addr = (mo_code_off + pos);
    if ((ty == 0)) {
      gl_idx = (-1);
      j = 0;
      while ((j < gl_count)) {
        if (mo_packed_eq((*(long long*)((uintptr_t)((gl_names + (j * 8))))), sym)) {
          gl_idx = j;
        }
        j = (j + 1);
      }
      if ((gl_idx >= 0)) {
        target_addr = (mo_data_off + (gl_idx * 8));
      } else {
        diag_str((long long)"internal error: global '");
        diag_packed(sym);
        diag_str((long long)"' not found in data section\n");
        bpp_sys_exit(2);
      }
    }
    if ((ty == 1)) {
      target_addr = (((mo_data_off + (gl_count * 8)) + (arr_len(mo_flt_data) * 8)) + arr_get(mo_sdata_off, sym));
    }
    if ((ty == 2)) {
      target_addr = ((mo_data_off + (gl_count * 8)) + (sym * 8));
    }
    if ((ty == 3)) {
      gl_idx = mo_find_got(sym);
      if ((gl_idx >= 0)) {
        target_addr = ((mo_data_off + mo_got_data_off) + (gl_idx * 8));
      } else {
        diag_str((long long)"internal error: extern '");
        diag_packed(sym);
        diag_str((long long)"' not found in GOT\n");
        bpp_sys_exit(2);
      }
    }
    page_diff = ((target_addr / 4096) - (pc_addr / 4096));
    immlo = (page_diff & 3);
    immhi = ((page_diff >> 2) & 0x7FFFF);
    instr = enc_read32(pos);
    instr = (instr & 0x9F00001F);
    instr = ((instr | (immlo << 29)) | (immhi << 5));
    enc_patch32(pos, instr);
    if ((ty == 3)) {
      imm12 = ((target_addr & 0xFFF) / 8);
    } else {
      imm12 = (target_addr & 0xFFF);
    }
    instr = enc_read32((pos + 4));
    instr = (instr & 0xFFC003FF);
    instr = (instr | (imm12 << 10));
    enc_patch32((pos + 4), instr);
    i = (i + 1);
  }
  return 0;
  return 0;
}

uint8_t write_macho(long long filename, long long main_label, long long gl_names, long long gl_count) {
  long long fd, written, code_size, main_code_off;
  long long nseg, has_data, i, et_actual, flt_bits;
  code_size = enc_pos;
  main_code_off = arr_get(enc_lbl_off, main_label);
  mo_compute_layout(code_size, gl_count);
  mo_main_off = (mo_code_off + main_code_off);
  mo_resolve_relocations(gl_names, gl_count);
  has_data = 0;
  if ((gl_count > 0)) {
    has_data = 1;
  }
  if ((arr_len(mo_flt_data) > 0)) {
    has_data = 1;
  }
  if ((mo_sdata_size > 0)) {
    has_data = 1;
  }
  if ((arr_len(mo_got_syms) > 0)) {
    has_data = 1;
  }
  nseg = (3 + has_data);
  mo_pos = 0;
  mo_write_header();
  mo_write_segment((long long)"__PAGEZERO", 10, 0, 0, 0, 0, 0, 0, 0, 0);
  (*(uint8_t*)((uintptr_t)((mo_buf + 64))) = (uint8_t)(0));
  (*(uint8_t*)((uintptr_t)((mo_buf + 65))) = (uint8_t)(0));
  (*(uint8_t*)((uintptr_t)((mo_buf + 66))) = (uint8_t)(0));
  (*(uint8_t*)((uintptr_t)((mo_buf + 67))) = (uint8_t)(0));
  (*(uint8_t*)((uintptr_t)((mo_buf + 68))) = (uint8_t)(1));
  (*(uint8_t*)((uintptr_t)((mo_buf + 69))) = (uint8_t)(0));
  (*(uint8_t*)((uintptr_t)((mo_buf + 70))) = (uint8_t)(0));
  (*(uint8_t*)((uintptr_t)((mo_buf + 71))) = (uint8_t)(0));
  mo_write_segment((long long)"__TEXT", 6, MO_VM_BASE, MO_VM_BASE_HI, mo_text_vmsize, 0, mo_text_vmsize, 5, 5, 1);
  mo_write_section((long long)"__text", 6, (long long)"__TEXT", 6, (MO_VM_BASE + mo_code_off), MO_VM_BASE_HI, code_size, mo_code_off, 2, 0x80000400);
  if (has_data) {
    mo_write_segment((long long)"__DATA", 6, mo_data_vmaddr_lo, mo_data_vmaddr_hi, mo_data_vmsize, mo_data_off, mo_data_vmsize, 3, 3, 1);
    mo_write_section((long long)"__data", 6, (long long)"__DATA", 6, mo_data_vmaddr_lo, mo_data_vmaddr_hi, mo_data_size, mo_data_off, 3, 0);
  }
  mo_write_segment((long long)"__LINKEDIT", 10, mo_link_vmaddr_lo, mo_link_vmaddr_hi, mo_link_vmsize, mo_link_off, mo_link_size, 1, 1, 0);
  mo_write_lc_chained_fixups();
  mo_write_lc_exports_trie();
  mo_write_lc_symtab();
  mo_write_lc_dysymtab();
  mo_write_lc_dylinker();
  mo_write_lc_main();
  mo_write_lc_load_dylib();
  i = 0;
  while ((i < arr_len(mo_ext_libs))) {
    mo_write_lc_load_dylib_named(arr_get(mo_ext_libs, i));
    i = (i + 1);
  }
  mo_w32(0x1D);
  mo_w32(16);
  mo_w32(mo_cs_off);
  mo_w32(mo_cs_size);
  mo_pad((mo_code_off - mo_pos));
  i = 0;
  while ((i < code_size)) {
    (*(uint8_t*)((uintptr_t)((mo_buf + mo_pos))) = (uint8_t)(((long long)(*(uint8_t*)((uintptr_t)((enc_buf + i)))))));
    mo_pos = (mo_pos + 1);
    i = (i + 1);
  }
  if (has_data) {
    mo_pad((mo_data_off - mo_pos));
    mo_pad((gl_count * 8));
    i = 0;
    while ((i < arr_len(mo_flt_data))) {
      flt_bits = arr_get(mo_flt_data, i);
      mo_w32((flt_bits & 0xFFFFFFFF));
      mo_w32(((flt_bits >> 32) & 0xFFFFFFFF));
      i = (i + 1);
    }
    i = 0;
    while ((i < mo_sdata_size)) {
      (*(uint8_t*)((uintptr_t)((mo_buf + mo_pos))) = (uint8_t)(((long long)(*(uint8_t*)((uintptr_t)((mo_sdata + i)))))));
      mo_pos = (mo_pos + 1);
      i = (i + 1);
    }
    while ((((mo_pos - mo_data_off) % 8) != 0)) {
      (*(uint8_t*)((uintptr_t)((mo_buf + mo_pos))) = (uint8_t)(0));
      mo_pos = (mo_pos + 1);
    }
    i = 0;
    while ((i < arr_len(mo_got_syms))) {
      mo_w32(i);
      if ((i < (arr_len(mo_got_syms) - 1))) {
        mo_w32(0x80100000);
      } else {
        mo_w32(0x80000000);
      }
      i = (i + 1);
    }
  }
  mo_pad((mo_link_off - mo_pos));
  if ((arr_len(mo_got_syms) > 0)) {
    mo_write_chained_fixups_real(nseg, gl_count);
  } else {
    mo_write_chained_fixups(nseg);
  }
  et_actual = mo_write_exports_trie(mo_main_off);
  if ((et_actual < mo_et_size)) {
    mo_pad((mo_et_size - et_actual));
  }
  mo_write_nlist(2, 0x0F, 1, 0x0010, MO_VM_BASE, MO_VM_BASE_HI);
  mo_write_nlist(22, 0x0F, 1, 0x0000, (MO_VM_BASE + mo_main_off), MO_VM_BASE_HI);
  mo_w8(0x20);
  mo_w8(0);
  mo_wcstr((long long)"__mh_execute_header", 19);
  mo_w8(0);
  mo_wcstr((long long)"_main", 5);
  mo_w8(0);
  mo_pad(4);
  mo_pad((mo_cs_off - mo_pos));
  mo_write_code_signature();
  fd = bpp_sys_open((const char*)((uintptr_t)(filename)), 0x601);
  written = bpp_sys_write(fd, (void*)((uintptr_t)(mo_buf)), mo_pos);
  bpp_sys_close(fd);
  return 0;
  return 0;
}

uint8_t mo_write_code_signature(void) {
  long long n_slots, n_special, id_len, hash_off, cd_len, sb_len;
  long long i, page_start, page_len, digest;
  long long cs_start, j;
  cs_start = mo_pos;
  n_slots = ((mo_cs_off + 4095) / 4096);
  n_special = 2;
  id_len = 6;
  hash_off = ((88 + id_len) + (n_special * 32));
  cd_len = (hash_off + (n_slots * 32));
  sb_len = (20 + cd_len);
  mo_w32be(0xFADE0CC0);
  mo_w32be(sb_len);
  mo_w32be(1);
  mo_w32be(0);
  mo_w32be(20);
  mo_w32be(0xFADE0C02);
  mo_w32be(cd_len);
  mo_w32be(0x00020100);
  mo_w32be(0x00000002);
  mo_w32be(hash_off);
  mo_w32be(88);
  mo_w32be(n_special);
  mo_w32be(n_slots);
  mo_w32be(mo_cs_off);
  mo_w8(32);
  mo_w8(2);
  mo_w8(0);
  mo_w8(12);
  mo_w32be(0);
  mo_w32be(0);
  mo_w32be(0);
  mo_w32be(0);
  mo_w64be(0);
  mo_w64be(0);
  mo_w64be(mo_text_vmsize);
  mo_w64be(1);
  mo_wcstr((long long)"a.out", 5);
  mo_w8(0);
  j = 0;
  while ((j < (n_special * 32))) {
    mo_w8(0);
    j = (j + 1);
  }
  digest = (long long)malloc(32);
  i = 0;
  while ((i < n_slots)) {
    page_start = (i * 4096);
    page_len = 4096;
    if (((page_start + page_len) > mo_cs_off)) {
      page_len = (mo_cs_off - page_start);
    }
    sha256((mo_buf + page_start), page_len, digest);
    j = 0;
    while ((j < 32)) {
      mo_w8(((long long)(*(uint8_t*)((uintptr_t)((digest + j))))));
      j = (j + 1);
    }
    i = (i + 1);
  }
  (free((void*)(uintptr_t)(digest)), 0);
  return 0;
  return 0;
}

uint8_t mo_w32be(long long v) {
  (*(uint8_t*)((uintptr_t)((mo_buf + mo_pos))) = (uint8_t)(((v >> 24) & 0xFF)));
  (*(uint8_t*)((uintptr_t)(((mo_buf + mo_pos) + 1))) = (uint8_t)(((v >> 16) & 0xFF)));
  (*(uint8_t*)((uintptr_t)(((mo_buf + mo_pos) + 2))) = (uint8_t)(((v >> 8) & 0xFF)));
  (*(uint8_t*)((uintptr_t)(((mo_buf + mo_pos) + 3))) = (uint8_t)((v & 0xFF)));
  mo_pos = (mo_pos + 4);
  return 0;
  return 0;
}

uint8_t mo_w64be(long long v) {
  mo_w32be(((v >> 32) & 0xFFFFFFFF));
  mo_w32be((v & 0xFFFFFFFF));
  return 0;
  return 0;
}

uint8_t init_x64_enc(void) {
  x64_enc_buf = (long long)malloc(1048576);
  x64_enc_pos = 0;
  x64_enc_lbl_off = arr_new();
  x64_enc_fix_pos = arr_new();
  x64_enc_fix_lbl = arr_new();
  x64_enc_rel_pos = arr_new();
  x64_enc_rel_sym = arr_new();
  x64_enc_rel_ty = arr_new();
  CC_O = 0;
  CC_NO = 1;
  CC_B = 2;
  CC_AE = 3;
  CC_E = 4;
  CC_NE = 5;
  CC_BE = 6;
  CC_A = 7;
  CC_S = 8;
  CC_NS = 9;
  CC_P = 10;
  CC_NP = 11;
  CC_L = 12;
  CC_GE = 13;
  CC_LE = 14;
  CC_G = 15;
  return 0;
  return 0;
}

uint8_t x64_enc_emit8(long long val) {
  (*(uint8_t*)((uintptr_t)((x64_enc_buf + x64_enc_pos))) = (uint8_t)((val & 0xFF)));
  x64_enc_pos = (x64_enc_pos + 1);
  return 0;
  return 0;
}

uint8_t x64_enc_emit32(long long val) {
  (*(uint8_t*)((uintptr_t)((x64_enc_buf + x64_enc_pos))) = (uint8_t)((val & 0xFF)));
  (*(uint8_t*)((uintptr_t)(((x64_enc_buf + x64_enc_pos) + 1))) = (uint8_t)(((val >> 8) & 0xFF)));
  (*(uint8_t*)((uintptr_t)(((x64_enc_buf + x64_enc_pos) + 2))) = (uint8_t)(((val >> 16) & 0xFF)));
  (*(uint8_t*)((uintptr_t)(((x64_enc_buf + x64_enc_pos) + 3))) = (uint8_t)(((val >> 24) & 0xFF)));
  x64_enc_pos = (x64_enc_pos + 4);
  return 0;
  return 0;
}

uint8_t x64_enc_emit64(long long val) {
  x64_enc_emit32((val & 0xFFFFFFFF));
  x64_enc_emit32(((val >> 32) & 0xFFFFFFFF));
  return 0;
  return 0;
}

uint8_t x64_enc_patch32(long long off, long long val) {
  (*(uint8_t*)((uintptr_t)((x64_enc_buf + off))) = (uint8_t)((val & 0xFF)));
  (*(uint8_t*)((uintptr_t)(((x64_enc_buf + off) + 1))) = (uint8_t)(((val >> 8) & 0xFF)));
  (*(uint8_t*)((uintptr_t)(((x64_enc_buf + off) + 2))) = (uint8_t)(((val >> 16) & 0xFF)));
  (*(uint8_t*)((uintptr_t)(((x64_enc_buf + off) + 3))) = (uint8_t)(((val >> 24) & 0xFF)));
  return 0;
  return 0;
}

long long x64_enc_read32(long long off) {
  return (((((long long)(*(uint8_t*)((uintptr_t)((x64_enc_buf + off))))) | (((long long)(*(uint8_t*)((uintptr_t)(((x64_enc_buf + off) + 1))))) << 8)) | (((long long)(*(uint8_t*)((uintptr_t)(((x64_enc_buf + off) + 2))))) << 16)) | (((long long)(*(uint8_t*)((uintptr_t)(((x64_enc_buf + off) + 3))))) << 24));
  return 0;
}

uint8_t x64_enc_def_label(long long id) {
  arr_set(x64_enc_lbl_off, id, x64_enc_pos);
  return 0;
  return 0;
}

uint8_t x64_enc_def_label_at(long long id, long long off) {
  arr_set(x64_enc_lbl_off, id, off);
  return 0;
  return 0;
}

long long x64_enc_new_label(void) {
  long long id;
  id = arr_len(x64_enc_lbl_off);
  x64_enc_lbl_off = arr_push(x64_enc_lbl_off, (0 - 1));
  return id;
  return 0;
}

uint8_t x64_enc_add_fixup(long long lbl) {
  x64_enc_fix_pos = arr_push(x64_enc_fix_pos, x64_enc_pos);
  x64_enc_fix_lbl = arr_push(x64_enc_fix_lbl, lbl);
  return 0;
  return 0;
}

uint8_t x64_enc_add_reloc(long long pos, long long sym, long long ty) {
  x64_enc_rel_pos = arr_push(x64_enc_rel_pos, pos);
  x64_enc_rel_sym = arr_push(x64_enc_rel_sym, sym);
  x64_enc_rel_ty = arr_push(x64_enc_rel_ty, ty);
  return 0;
  return 0;
}

uint8_t x64_enc_rex(long long w, long long r, long long x, long long b) {
  x64_enc_emit8(((((0x40 | (w << 3)) | (r << 2)) | (x << 1)) | b));
  return 0;
  return 0;
}

uint8_t x64_enc_modrm(long long mod, long long reg, long long rm) {
  x64_enc_emit8((((mod << 6) | ((reg & 7) << 3)) | (rm & 7)));
  return 0;
  return 0;
}

uint8_t x64_enc_mem(long long reg_field, long long base, long long disp) {
  long long r, b3, mod, need_sib;
  r = (reg_field & 7);
  b3 = (base & 7);
  need_sib = 0;
  if ((b3 == 4)) {
    need_sib = 1;
  }
  if ((disp == 0)) {
    if ((b3 == 5)) {
      mod = 1;
    } else {
      mod = 0;
    }
  } else {
    if ((disp >= (-128))) {
      if ((disp < 128)) {
        mod = 1;
      } else {
        mod = 2;
      }
    } else {
      mod = 2;
    }
  }
  if (need_sib) {
    x64_enc_emit8((((mod << 6) | (r << 3)) | 4));
    x64_enc_emit8(0x24);
  } else {
    x64_enc_emit8((((mod << 6) | (r << 3)) | b3));
  }
  if ((mod == 1)) {
    x64_enc_emit8((disp & 0xFF));
  }
  if ((mod == 2)) {
    x64_enc_emit32(disp);
  }
  return 0;
  return 0;
}

uint8_t x64_enc_mov_reg(long long dst, long long src) {
  x64_enc_rex(1, ((src >> 3) & 1), 0, ((dst >> 3) & 1));
  x64_enc_emit8(0x89);
  x64_enc_modrm(3, src, dst);
  return 0;
  return 0;
}

uint8_t x64_enc_mov_imm64(long long rd, long long imm) {
  x64_enc_rex(1, 0, 0, ((rd >> 3) & 1));
  x64_enc_emit8((0xB8 | (rd & 7)));
  x64_enc_emit64(imm);
  return 0;
  return 0;
}

uint8_t x64_enc_mov_imm32(long long rd, long long imm) {
  x64_enc_rex(1, 0, 0, ((rd >> 3) & 1));
  x64_enc_emit8(0xC7);
  x64_enc_modrm(3, 0, rd);
  x64_enc_emit32(imm);
  return 0;
  return 0;
}

uint8_t x64_enc_xor_zero(long long rd) {
  if ((rd >= 8)) {
    x64_enc_rex(0, ((rd >> 3) & 1), 0, ((rd >> 3) & 1));
  }
  x64_enc_emit8(0x31);
  x64_enc_modrm(3, rd, rd);
  return 0;
  return 0;
}

uint8_t x64_enc_mov_wide(long long rd, long long val) {
  if ((val == 0)) {
    x64_enc_xor_zero(rd);
    return 0;
  }
  if ((val > 0)) {
    if ((val < 0x100000000)) {
      if ((rd >= 8)) {
        x64_enc_emit8(0x41);
      }
      x64_enc_emit8((0xB8 | (rd & 7)));
      x64_enc_emit32(val);
      return 0;
    }
  }
  x64_enc_mov_imm64(rd, val);
  return 0;
  return 0;
}

uint8_t x64_enc_add_reg(long long dst, long long src) {
  x64_enc_rex(1, ((src >> 3) & 1), 0, ((dst >> 3) & 1));
  x64_enc_emit8(0x01);
  x64_enc_modrm(3, src, dst);
  return 0;
  return 0;
}

uint8_t x64_enc_sub_reg(long long dst, long long src) {
  x64_enc_rex(1, ((src >> 3) & 1), 0, ((dst >> 3) & 1));
  x64_enc_emit8(0x29);
  x64_enc_modrm(3, src, dst);
  return 0;
  return 0;
}

uint8_t x64_enc_and_reg(long long dst, long long src) {
  x64_enc_rex(1, ((src >> 3) & 1), 0, ((dst >> 3) & 1));
  x64_enc_emit8(0x21);
  x64_enc_modrm(3, src, dst);
  return 0;
  return 0;
}

uint8_t x64_enc_or_reg(long long dst, long long src) {
  x64_enc_rex(1, ((src >> 3) & 1), 0, ((dst >> 3) & 1));
  x64_enc_emit8(0x09);
  x64_enc_modrm(3, src, dst);
  return 0;
  return 0;
}

uint8_t x64_enc_xor_reg(long long dst, long long src) {
  x64_enc_rex(1, ((src >> 3) & 1), 0, ((dst >> 3) & 1));
  x64_enc_emit8(0x31);
  x64_enc_modrm(3, src, dst);
  return 0;
  return 0;
}

uint8_t x64_enc_cmp_reg(long long dst, long long src) {
  x64_enc_rex(1, ((src >> 3) & 1), 0, ((dst >> 3) & 1));
  x64_enc_emit8(0x39);
  x64_enc_modrm(3, src, dst);
  return 0;
  return 0;
}

uint8_t x64_enc_test_reg(long long dst, long long src) {
  x64_enc_rex(1, ((src >> 3) & 1), 0, ((dst >> 3) & 1));
  x64_enc_emit8(0x85);
  x64_enc_modrm(3, src, dst);
  return 0;
  return 0;
}

uint8_t x64_enc_add_imm(long long rd, long long imm32) {
  x64_enc_rex(1, 0, 0, ((rd >> 3) & 1));
  x64_enc_emit8(0x81);
  x64_enc_modrm(3, 0, rd);
  x64_enc_emit32(imm32);
  return 0;
  return 0;
}

uint8_t x64_enc_sub_imm(long long rd, long long imm32) {
  x64_enc_rex(1, 0, 0, ((rd >> 3) & 1));
  x64_enc_emit8(0x81);
  x64_enc_modrm(3, 5, rd);
  x64_enc_emit32(imm32);
  return 0;
  return 0;
}

uint8_t x64_enc_cmp_imm(long long rd, long long imm32) {
  x64_enc_rex(1, 0, 0, ((rd >> 3) & 1));
  x64_enc_emit8(0x81);
  x64_enc_modrm(3, 7, rd);
  x64_enc_emit32(imm32);
  return 0;
  return 0;
}

uint8_t x64_enc_and_imm(long long rd, long long imm32) {
  x64_enc_rex(1, 0, 0, ((rd >> 3) & 1));
  x64_enc_emit8(0x81);
  x64_enc_modrm(3, 4, rd);
  x64_enc_emit32(imm32);
  return 0;
  return 0;
}

uint8_t x64_enc_or_imm(long long rd, long long imm32) {
  x64_enc_rex(1, 0, 0, ((rd >> 3) & 1));
  x64_enc_emit8(0x81);
  x64_enc_modrm(3, 1, rd);
  x64_enc_emit32(imm32);
  return 0;
  return 0;
}

uint8_t x64_enc_xor_imm(long long rd, long long imm32) {
  x64_enc_rex(1, 0, 0, ((rd >> 3) & 1));
  x64_enc_emit8(0x81);
  x64_enc_modrm(3, 6, rd);
  x64_enc_emit32(imm32);
  return 0;
  return 0;
}

uint8_t x64_enc_neg(long long rd) {
  x64_enc_rex(1, 0, 0, ((rd >> 3) & 1));
  x64_enc_emit8(0xF7);
  x64_enc_modrm(3, 3, rd);
  return 0;
  return 0;
}

uint8_t x64_enc_not(long long rd) {
  x64_enc_rex(1, 0, 0, ((rd >> 3) & 1));
  x64_enc_emit8(0xF7);
  x64_enc_modrm(3, 2, rd);
  return 0;
  return 0;
}

uint8_t x64_enc_imul(long long dst, long long src) {
  x64_enc_rex(1, ((dst >> 3) & 1), 0, ((src >> 3) & 1));
  x64_enc_emit8(0x0F);
  x64_enc_emit8(0xAF);
  x64_enc_modrm(3, dst, src);
  return 0;
  return 0;
}

uint8_t x64_enc_idiv(long long src) {
  x64_enc_rex(1, 0, 0, ((src >> 3) & 1));
  x64_enc_emit8(0xF7);
  x64_enc_modrm(3, 7, src);
  return 0;
  return 0;
}

uint8_t x64_enc_cqo(void) {
  x64_enc_rex(1, 0, 0, 0);
  x64_enc_emit8(0x99);
  return 0;
  return 0;
}

uint8_t x64_enc_shl_cl(long long rd) {
  x64_enc_rex(1, 0, 0, ((rd >> 3) & 1));
  x64_enc_emit8(0xD3);
  x64_enc_modrm(3, 4, rd);
  return 0;
  return 0;
}

uint8_t x64_enc_shr_cl(long long rd) {
  x64_enc_rex(1, 0, 0, ((rd >> 3) & 1));
  x64_enc_emit8(0xD3);
  x64_enc_modrm(3, 5, rd);
  return 0;
  return 0;
}

uint8_t x64_enc_sar_cl(long long rd) {
  x64_enc_rex(1, 0, 0, ((rd >> 3) & 1));
  x64_enc_emit8(0xD3);
  x64_enc_modrm(3, 7, rd);
  return 0;
  return 0;
}

uint8_t x64_enc_shl_imm(long long rd, long long imm8) {
  x64_enc_rex(1, 0, 0, ((rd >> 3) & 1));
  x64_enc_emit8(0xC1);
  x64_enc_modrm(3, 4, rd);
  x64_enc_emit8((imm8 & 0x3F));
  return 0;
  return 0;
}

uint8_t x64_enc_shr_imm(long long rd, long long imm8) {
  x64_enc_rex(1, 0, 0, ((rd >> 3) & 1));
  x64_enc_emit8(0xC1);
  x64_enc_modrm(3, 5, rd);
  x64_enc_emit8((imm8 & 0x3F));
  return 0;
  return 0;
}

uint8_t x64_enc_sar_imm(long long rd, long long imm8) {
  x64_enc_rex(1, 0, 0, ((rd >> 3) & 1));
  x64_enc_emit8(0xC1);
  x64_enc_modrm(3, 7, rd);
  x64_enc_emit8((imm8 & 0x3F));
  return 0;
  return 0;
}

uint8_t x64_enc_push(long long rd) {
  if ((rd >= 8)) {
    x64_enc_emit8(0x41);
  }
  x64_enc_emit8((0x50 | (rd & 7)));
  return 0;
  return 0;
}

uint8_t x64_enc_pop(long long rd) {
  if ((rd >= 8)) {
    x64_enc_emit8(0x41);
  }
  x64_enc_emit8((0x58 | (rd & 7)));
  return 0;
  return 0;
}

uint8_t x64_enc_load(long long dst, long long base, long long disp) {
  x64_enc_rex(1, ((dst >> 3) & 1), 0, ((base >> 3) & 1));
  x64_enc_emit8(0x8B);
  x64_enc_mem(dst, base, disp);
  return 0;
  return 0;
}

uint8_t x64_enc_store(long long src, long long base, long long disp) {
  x64_enc_rex(1, ((src >> 3) & 1), 0, ((base >> 3) & 1));
  x64_enc_emit8(0x89);
  x64_enc_mem(src, base, disp);
  return 0;
  return 0;
}

uint8_t x64_enc_loadb(long long dst, long long base, long long disp) {
  x64_enc_rex(1, ((dst >> 3) & 1), 0, ((base >> 3) & 1));
  x64_enc_emit8(0x0F);
  x64_enc_emit8(0xB6);
  x64_enc_mem(dst, base, disp);
  return 0;
  return 0;
}

uint8_t x64_enc_storeb(long long src, long long base, long long disp) {
  x64_enc_rex(0, ((src >> 3) & 1), 0, ((base >> 3) & 1));
  x64_enc_emit8(0x88);
  x64_enc_mem(src, base, disp);
  return 0;
  return 0;
}

uint8_t x64_enc_loadh(long long dst, long long base, long long disp) {
  x64_enc_rex(1, ((dst >> 3) & 1), 0, ((base >> 3) & 1));
  x64_enc_emit8(0x0F);
  x64_enc_emit8(0xB7);
  x64_enc_mem(dst, base, disp);
  return 0;
  return 0;
}

uint8_t x64_enc_storeh(long long src, long long base, long long disp) {
  x64_enc_emit8(0x66);
  x64_enc_rex(0, ((src >> 3) & 1), 0, ((base >> 3) & 1));
  x64_enc_emit8(0x89);
  x64_enc_mem(src, base, disp);
  return 0;
  return 0;
}

uint8_t x64_enc_load_w(long long dst, long long base, long long disp) {
  x64_enc_rex(0, ((dst >> 3) & 1), 0, ((base >> 3) & 1));
  x64_enc_emit8(0x8B);
  x64_enc_mem(dst, base, disp);
  return 0;
  return 0;
}

uint8_t x64_enc_store_w(long long src, long long base, long long disp) {
  x64_enc_rex(0, ((src >> 3) & 1), 0, ((base >> 3) & 1));
  x64_enc_emit8(0x89);
  x64_enc_mem(src, base, disp);
  return 0;
  return 0;
}

uint8_t x64_enc_lea(long long dst, long long base, long long disp) {
  x64_enc_rex(1, ((dst >> 3) & 1), 0, ((base >> 3) & 1));
  x64_enc_emit8(0x8D);
  x64_enc_mem(dst, base, disp);
  return 0;
  return 0;
}

uint8_t x64_enc_call_label(long long lbl) {
  long long target, rel;
  target = arr_get(x64_enc_lbl_off, lbl);
  x64_enc_emit8(0xE8);
  if ((target >= 0)) {
    rel = (target - (x64_enc_pos + 4));
    x64_enc_emit32(rel);
  } else {
    x64_enc_add_fixup(lbl);
    x64_enc_emit32(0);
  }
  return 0;
  return 0;
}

uint8_t x64_enc_jmp_label(long long lbl) {
  long long target, rel;
  target = arr_get(x64_enc_lbl_off, lbl);
  x64_enc_emit8(0xE9);
  if ((target >= 0)) {
    rel = (target - (x64_enc_pos + 4));
    x64_enc_emit32(rel);
  } else {
    x64_enc_add_fixup(lbl);
    x64_enc_emit32(0);
  }
  return 0;
  return 0;
}

uint8_t x64_enc_jcc_label(long long cc, long long lbl) {
  long long target, rel;
  target = arr_get(x64_enc_lbl_off, lbl);
  x64_enc_emit8(0x0F);
  x64_enc_emit8((0x80 | cc));
  if ((target >= 0)) {
    rel = (target - (x64_enc_pos + 4));
    x64_enc_emit32(rel);
  } else {
    x64_enc_add_fixup(lbl);
    x64_enc_emit32(0);
  }
  return 0;
  return 0;
}

uint8_t x64_enc_call_reg(long long rd) {
  if ((rd >= 8)) {
    x64_enc_emit8(0x41);
  }
  x64_enc_emit8(0xFF);
  x64_enc_modrm(3, 2, rd);
  return 0;
  return 0;
}

uint8_t x64_enc_jmp_reg(long long rd) {
  if ((rd >= 8)) {
    x64_enc_emit8(0x41);
  }
  x64_enc_emit8(0xFF);
  x64_enc_modrm(3, 4, rd);
  return 0;
  return 0;
}

uint8_t x64_enc_ret(void) {
  x64_enc_emit8(0xC3);
  return 0;
  return 0;
}

uint8_t x64_enc_syscall(void) {
  x64_enc_emit8(0x0F);
  x64_enc_emit8(0x05);
  return 0;
  return 0;
}

uint8_t x64_enc_nop(void) {
  x64_enc_emit8(0x90);
  return 0;
  return 0;
}

uint8_t x64_enc_setcc(long long cc, long long rd) {
  x64_enc_rex(0, 0, 0, ((rd >> 3) & 1));
  x64_enc_emit8(0x0F);
  x64_enc_emit8((0x90 | cc));
  x64_enc_modrm(3, 0, rd);
  return 0;
  return 0;
}

uint8_t x64_enc_movzx8(long long dst, long long src) {
  long long need_rex;
  need_rex = 0;
  if ((dst >= 8)) {
    need_rex = 1;
  }
  if ((src >= 4)) {
    need_rex = 1;
  }
  if (need_rex) {
    x64_enc_rex(0, ((dst >> 3) & 1), 0, ((src >> 3) & 1));
  }
  x64_enc_emit8(0x0F);
  x64_enc_emit8(0xB6);
  x64_enc_modrm(3, dst, src);
  return 0;
  return 0;
}

long long x64_enc_lea_rip(long long rd) {
  long long disp_pos;
  x64_enc_rex(1, ((rd >> 3) & 1), 0, 0);
  x64_enc_emit8(0x8D);
  x64_enc_modrm(0, rd, 5);
  disp_pos = x64_enc_pos;
  x64_enc_emit32(0);
  return disp_pos;
  return 0;
}

long long x64_enc_load_rip(long long rd) {
  long long disp_pos;
  x64_enc_rex(1, ((rd >> 3) & 1), 0, 0);
  x64_enc_emit8(0x8B);
  x64_enc_modrm(0, rd, 5);
  disp_pos = x64_enc_pos;
  x64_enc_emit32(0);
  return disp_pos;
  return 0;
}

uint8_t x64_enc_addsd(long long dst, long long src) {
  x64_enc_emit8(0xF2);
  if (((dst >= 8) | (src >= 8))) {
    x64_enc_rex(0, ((dst >> 3) & 1), 0, ((src >> 3) & 1));
  }
  x64_enc_emit8(0x0F);
  x64_enc_emit8(0x58);
  x64_enc_modrm(3, dst, src);
  return 0;
  return 0;
}

uint8_t x64_enc_subsd(long long dst, long long src) {
  x64_enc_emit8(0xF2);
  if (((dst >= 8) | (src >= 8))) {
    x64_enc_rex(0, ((dst >> 3) & 1), 0, ((src >> 3) & 1));
  }
  x64_enc_emit8(0x0F);
  x64_enc_emit8(0x5C);
  x64_enc_modrm(3, dst, src);
  return 0;
  return 0;
}

uint8_t x64_enc_mulsd(long long dst, long long src) {
  x64_enc_emit8(0xF2);
  if (((dst >= 8) | (src >= 8))) {
    x64_enc_rex(0, ((dst >> 3) & 1), 0, ((src >> 3) & 1));
  }
  x64_enc_emit8(0x0F);
  x64_enc_emit8(0x59);
  x64_enc_modrm(3, dst, src);
  return 0;
  return 0;
}

uint8_t x64_enc_divsd(long long dst, long long src) {
  x64_enc_emit8(0xF2);
  if (((dst >= 8) | (src >= 8))) {
    x64_enc_rex(0, ((dst >> 3) & 1), 0, ((src >> 3) & 1));
  }
  x64_enc_emit8(0x0F);
  x64_enc_emit8(0x5E);
  x64_enc_modrm(3, dst, src);
  return 0;
  return 0;
}

uint8_t x64_enc_ucomisd(long long dst, long long src) {
  x64_enc_emit8(0x66);
  if (((dst >= 8) | (src >= 8))) {
    x64_enc_rex(0, ((dst >> 3) & 1), 0, ((src >> 3) & 1));
  }
  x64_enc_emit8(0x0F);
  x64_enc_emit8(0x2E);
  x64_enc_modrm(3, dst, src);
  return 0;
  return 0;
}

uint8_t x64_enc_movsd_reg(long long dst, long long src) {
  x64_enc_emit8(0xF2);
  if (((dst >= 8) | (src >= 8))) {
    x64_enc_rex(0, ((dst >> 3) & 1), 0, ((src >> 3) & 1));
  }
  x64_enc_emit8(0x0F);
  x64_enc_emit8(0x10);
  x64_enc_modrm(3, dst, src);
  return 0;
  return 0;
}

uint8_t x64_enc_xorpd(long long dst, long long src) {
  x64_enc_emit8(0x66);
  if (((dst >= 8) | (src >= 8))) {
    x64_enc_rex(0, ((dst >> 3) & 1), 0, ((src >> 3) & 1));
  }
  x64_enc_emit8(0x0F);
  x64_enc_emit8(0x57);
  x64_enc_modrm(3, dst, src);
  return 0;
  return 0;
}

uint8_t x64_enc_cvtsi2sd(long long dst, long long src) {
  x64_enc_emit8(0xF2);
  x64_enc_rex(1, ((dst >> 3) & 1), 0, ((src >> 3) & 1));
  x64_enc_emit8(0x0F);
  x64_enc_emit8(0x2A);
  x64_enc_modrm(3, dst, src);
  return 0;
  return 0;
}

uint8_t x64_enc_cvttsd2si(long long dst, long long src) {
  x64_enc_emit8(0xF2);
  x64_enc_rex(1, ((dst >> 3) & 1), 0, ((src >> 3) & 1));
  x64_enc_emit8(0x0F);
  x64_enc_emit8(0x2C);
  x64_enc_modrm(3, dst, src);
  return 0;
  return 0;
}

uint8_t x64_enc_movq_to_xmm(long long dst, long long src) {
  x64_enc_emit8(0x66);
  x64_enc_rex(1, ((dst >> 3) & 1), 0, ((src >> 3) & 1));
  x64_enc_emit8(0x0F);
  x64_enc_emit8(0x6E);
  x64_enc_modrm(3, dst, src);
  return 0;
  return 0;
}

uint8_t x64_enc_movq_from_xmm(long long dst, long long src) {
  x64_enc_emit8(0x66);
  x64_enc_rex(1, ((src >> 3) & 1), 0, ((dst >> 3) & 1));
  x64_enc_emit8(0x0F);
  x64_enc_emit8(0x7E);
  x64_enc_modrm(3, src, dst);
  return 0;
  return 0;
}

uint8_t x64_enc_load_sd(long long dst, long long base, long long disp) {
  x64_enc_emit8(0xF2);
  if (((dst >= 8) | (base >= 8))) {
    x64_enc_rex(0, ((dst >> 3) & 1), 0, ((base >> 3) & 1));
  }
  x64_enc_emit8(0x0F);
  x64_enc_emit8(0x10);
  x64_enc_mem(dst, base, disp);
  return 0;
  return 0;
}

uint8_t x64_enc_store_sd(long long src, long long base, long long disp) {
  x64_enc_emit8(0xF2);
  if (((src >= 8) | (base >= 8))) {
    x64_enc_rex(0, ((src >> 3) & 1), 0, ((base >> 3) & 1));
  }
  x64_enc_emit8(0x0F);
  x64_enc_emit8(0x11);
  x64_enc_mem(src, base, disp);
  return 0;
  return 0;
}

long long x64_enc_load_sd_rip(long long dst) {
  long long disp_pos;
  x64_enc_emit8(0xF2);
  if ((dst >= 8)) {
    x64_enc_rex(0, ((dst >> 3) & 1), 0, 0);
  }
  x64_enc_emit8(0x0F);
  x64_enc_emit8(0x10);
  x64_enc_modrm(0, dst, 5);
  disp_pos = x64_enc_pos;
  x64_enc_emit32(0);
  return disp_pos;
  return 0;
}

uint8_t x64_enc_resolve_fixups(void) {
  long long i, pos, lbl, target, off;
  i = 0;
  while ((i < arr_len(x64_enc_fix_pos))) {
    pos = arr_get(x64_enc_fix_pos, i);
    lbl = arr_get(x64_enc_fix_lbl, i);
    target = arr_get(x64_enc_lbl_off, lbl);
    off = (target - (pos + 4));
    x64_enc_patch32(pos, off);
    i = (i + 1);
  }
  return 0;
  return 0;
}

uint8_t init_codegen_x86_64(void) {
  x64_fn_name = arr_new();
  x64_fn_par = arr_new();
  x64_fn_pcnt = arr_new();
  x64_fn_body = arr_new();
  x64_fn_bcnt = arr_new();
  x64_fn_fidx = arr_new();
  x64_gl_name = arr_new();
  x64_ex_name = arr_new();
  x64_ex_ret = arr_new();
  x64_ex_args = arr_new();
  x64_ex_acnt = arr_new();
  x64_vars = arr_new();
  x64_var_stack = arr_new();
  x64_var_struct_idx = arr_new();
  x64_var_off = arr_new();
  x64_var_forced_ty = arr_new();
  x64_flt_tbl = arr_new();
  x64_str_tbl = arr_new();
  x64_lbl_cnt = 0;
  x64_depth = 0;
  x64_break_stack = arr_new();
  x64_fn_lbl = arr_new();
  x64_ret_lbl = 0;
  x64_argc_sym = (0 - 1);
  x64_argv_sym = (0 - 2);
  return 0;
  return 0;
}

uint8_t bridge_data_x86_64(void) {
  long long i, rec, name_p, idx, j;
  x64_sbuf = vbuf;
  arr_clear(x64_fn_name);
  arr_clear(x64_fn_par);
  arr_clear(x64_fn_pcnt);
  arr_clear(x64_fn_body);
  arr_clear(x64_fn_bcnt);
  arr_clear(x64_fn_fidx);
  i = 0;
  while ((i < func_cnt)) {
    rec = (*(long long*)((uintptr_t)((funcs + (i * 8)))));
    name_p = (*(long long*)((uintptr_t)((rec + FN_NAME))));
    idx = (-1);
    j = 0;
    while ((j < arr_len(x64_fn_name))) {
      if (x64_str_eq_packed(name_p, arr_get(x64_fn_name, j))) {
        idx = j;
      }
      j = (j + 1);
    }
    if ((idx < 0)) {
      idx = arr_len(x64_fn_name);
      x64_fn_name = arr_push(x64_fn_name, name_p);
      x64_fn_par = arr_push(x64_fn_par, (*(long long*)((uintptr_t)((rec + FN_PARS)))));
      x64_fn_pcnt = arr_push(x64_fn_pcnt, (*(long long*)((uintptr_t)((rec + FN_PCNT)))));
      x64_fn_body = arr_push(x64_fn_body, (*(long long*)((uintptr_t)((rec + FN_BODY)))));
      x64_fn_bcnt = arr_push(x64_fn_bcnt, (*(long long*)((uintptr_t)((rec + FN_BCNT)))));
      x64_fn_fidx = arr_push(x64_fn_fidx, i);
    } else {
      arr_set(x64_fn_name, idx, name_p);
      arr_set(x64_fn_par, idx, (*(long long*)((uintptr_t)((rec + FN_PARS)))));
      arr_set(x64_fn_pcnt, idx, (*(long long*)((uintptr_t)((rec + FN_PCNT)))));
      arr_set(x64_fn_body, idx, (*(long long*)((uintptr_t)((rec + FN_BODY)))));
      arr_set(x64_fn_bcnt, idx, (*(long long*)((uintptr_t)((rec + FN_BCNT)))));
      arr_set(x64_fn_fidx, idx, i);
    }
    i = (i + 1);
  }
  arr_clear(x64_gl_name);
  i = 0;
  while ((i < glob_cnt)) {
    name_p = (*(long long*)((uintptr_t)((globals + (i * 8)))));
    idx = (-1);
    j = 0;
    while ((j < arr_len(x64_gl_name))) {
      if (x64_str_eq_packed(name_p, arr_get(x64_gl_name, j))) {
        idx = j;
      }
      j = (j + 1);
    }
    if ((idx < 0)) {
      x64_gl_name = arr_push(x64_gl_name, name_p);
    }
    i = (i + 1);
  }
  arr_clear(x64_ex_name);
  arr_clear(x64_ex_ret);
  arr_clear(x64_ex_args);
  arr_clear(x64_ex_acnt);
  i = 0;
  while ((i < ext_cnt)) {
    rec = (*(long long*)((uintptr_t)((externs + (i * 8)))));
    x64_ex_name = arr_push(x64_ex_name, (*(long long*)((uintptr_t)((rec + EX_NAME)))));
    x64_ex_ret = arr_push(x64_ex_ret, (*(long long*)((uintptr_t)((rec + EX_RET)))));
    x64_ex_args = arr_push(x64_ex_args, (*(long long*)((uintptr_t)((rec + EX_ARGS)))));
    x64_ex_acnt = arr_push(x64_ex_acnt, (*(long long*)((uintptr_t)((rec + EX_ACNT)))));
    i = (i + 1);
  }
  return 0;
  return 0;
}

uint8_t x64_str_eq(long long p, long long lit, long long litlen) {
  if ((unpack_l(p) != litlen)) {
    return 0;
  }
  return buf_eq((x64_sbuf + unpack_s(p)), lit, litlen);
  return 0;
}

uint8_t x64_str_eq_packed(long long a, long long b) {
  long long as, al, bs, bl, i;
  al = unpack_l(a);
  bl = unpack_l(b);
  if ((al != bl)) {
    return 0;
  }
  as = unpack_s(a);
  bs = unpack_s(b);
  i = 0;
  while ((i < al)) {
    if ((((long long)(*(uint8_t*)((uintptr_t)(((x64_sbuf + as) + i))))) != ((long long)(*(uint8_t*)((uintptr_t)(((x64_sbuf + bs) + i))))))) {
      return 0;
    }
    i = (i + 1);
  }
  return 1;
  return 0;
}

long long x64_var_add(long long name_p) {
  long long idx;
  idx = arr_len(x64_vars);
  x64_vars = arr_push(x64_vars, name_p);
  x64_var_stack = arr_push(x64_var_stack, 0);
  x64_var_struct_idx = arr_push(x64_var_struct_idx, (0 - 1));
  x64_var_off = arr_push(x64_var_off, 0);
  x64_var_forced_ty = arr_push(x64_var_forced_ty, 0);
  return idx;
  return 0;
}

uint8_t x64_var_idx(long long name_p) {
  long long i;
  i = 0;
  while ((i < arr_len(x64_vars))) {
    if (x64_str_eq_packed(name_p, arr_get(x64_vars, i))) {
      return i;
    }
    i = (i + 1);
  }
  return (-1);
  return 0;
}

long long x64_var_find(long long name_p) {
  long long i;
  i = x64_var_idx(name_p);
  if ((i >= 0)) {
    return arr_get(x64_var_off, i);
  }
  return (-1);
  return 0;
}

uint8_t x64_pre_reg_vars(long long arr, long long cnt) {
  long long i, j;
  long long n;
  i = 0;
  while ((i < cnt)) {
    n = (*(long long*)((uintptr_t)((arr + (i * 8)))));
    if ((n != 0)) {
      if (((*(long long*)((uintptr_t)((n + 0)))) == T_DECL)) {
        j = 0;
        while ((j < (*(long long*)((uintptr_t)((n + 16)))))) {
          long long vi;
          vi = x64_var_add((*(long long*)((uintptr_t)(((*(long long*)((uintptr_t)((n + 8)))) + (j * 8))))));
          if (((*(long long*)((uintptr_t)((n + 32)))) == 1)) {
            arr_set(x64_var_stack, vi, 1);
            arr_set(x64_var_struct_idx, vi, get_var_type((*(long long*)((uintptr_t)(((*(long long*)((uintptr_t)((n + 8)))) + (j * 8)))))));
          }
          if (((*(long long*)((uintptr_t)((n + 24)))) > 0)) {
            arr_set(x64_var_forced_ty, vi, (*(long long*)((uintptr_t)((n + 24)))));
          }
          j = (j + 1);
        }
      }
      if (((*(long long*)((uintptr_t)((n + 0)))) == T_IF)) {
        x64_pre_reg_vars((*(long long*)((uintptr_t)((n + 16)))), (*(long long*)((uintptr_t)((n + 24)))));
        if (((*(long long*)((uintptr_t)((n + 40)))) > 0)) {
          x64_pre_reg_vars((*(long long*)((uintptr_t)((n + 32)))), (*(long long*)((uintptr_t)((n + 40)))));
        }
      }
      if (((*(long long*)((uintptr_t)((n + 0)))) == T_WHILE)) {
        x64_pre_reg_vars((*(long long*)((uintptr_t)((n + 16)))), (*(long long*)((uintptr_t)((n + 24)))));
      }
    }
    i = (i + 1);
  }
  return 0;
  return 0;
}

long long x64_parse_int(long long p) {
  long long s, l, i, ch, val;
  s = unpack_s(p);
  l = unpack_l(p);
  val = 0;
  i = 0;
  if ((l > 2)) {
    if ((((long long)(*(uint8_t*)((uintptr_t)((x64_sbuf + s))))) == 48)) {
      if (((((long long)(*(uint8_t*)((uintptr_t)(((x64_sbuf + s) + 1))))) == 120) | (((long long)(*(uint8_t*)((uintptr_t)(((x64_sbuf + s) + 1))))) == 88))) {
        i = 2;
        while ((i < l)) {
          ch = ((long long)(*(uint8_t*)((uintptr_t)(((x64_sbuf + s) + i)))));
          val = (val * 16);
          if ((ch >= 48)) {
            if ((ch <= 57)) {
              val = ((val + ch) - 48);
            }
          }
          if ((ch >= 65)) {
            if ((ch <= 70)) {
              val = ((val + ch) - 55);
            }
          }
          if ((ch >= 97)) {
            if ((ch <= 102)) {
              val = ((val + ch) - 87);
            }
          }
          i = (i + 1);
        }
        return val;
      }
    }
  }
  while ((i < l)) {
    ch = ((long long)(*(uint8_t*)((uintptr_t)(((x64_sbuf + s) + i)))));
    val = (((val * 10) + ch) - 48);
    i = (i + 1);
  }
  return val;
  return 0;
}

uint8_t x64_is_float_lit(long long p) {
  long long s, l, i;
  s = unpack_s(p);
  l = unpack_l(p);
  i = 0;
  while ((i < l)) {
    if ((((long long)(*(uint8_t*)((uintptr_t)(((x64_sbuf + s) + i))))) == 46)) {
      return 1;
    }
    i = (i + 1);
  }
  return 0;
  return 0;
}

long long x64_var_is_float(long long vi) {
  return (get_fn_var_type(x64_cur_fn_idx, arr_get(x64_vars, vi)) == TY_FLOAT);
  return 0;
}

uint8_t x64_emit_load_var(long long vi, long long off) {
  long long fty;
  fty = arr_get(x64_var_forced_ty, vi);
  if (x64_var_is_float(vi)) {
    x64_enc_load_sd(0, 5, off);
    return 1;
  }
  if ((fty == TY_BYTE)) {
    x64_enc_loadb(0, 5, off);
    return 0;
  }
  if ((fty == TY_QUART)) {
    x64_enc_loadh(0, 5, off);
    return 0;
  }
  if ((fty == TY_HALF)) {
    x64_enc_load_w(0, 5, off);
    return 0;
  }
  x64_enc_load(0, 5, off);
  return 0;
  return 0;
}

uint8_t x64_emit_store_var(long long vi, long long off, long long rty) {
  long long fty;
  fty = arr_get(x64_var_forced_ty, vi);
  if (x64_var_is_float(vi)) {
    if ((rty == 0)) {
      x64_enc_cvtsi2sd(0, 0);
    }
    x64_enc_store_sd(0, 5, off);
    return 0;
  }
  if ((rty == 1)) {
    x64_enc_cvttsd2si(0, 0);
  }
  if ((fty == TY_BYTE)) {
    x64_enc_storeb(0, 5, off);
    return 0;
  }
  if ((fty == TY_QUART)) {
    x64_enc_storeh(0, 5, off);
    return 0;
  }
  if ((fty == TY_HALF)) {
    x64_enc_store_w(0, 5, off);
    return 0;
  }
  x64_enc_store(0, 5, off);
  return 0;
  return 0;
}

uint8_t x64_find_fn(long long name_p) {
  long long i;
  i = 0;
  while ((i < arr_len(x64_fn_name))) {
    if (x64_str_eq_packed(name_p, arr_get(x64_fn_name, i))) {
      return i;
    }
    i = (i + 1);
  }
  return (-1);
  return 0;
}

uint8_t x64_find_ext(long long name_p) {
  long long i;
  i = 0;
  while ((i < arr_len(x64_ex_name))) {
    if (x64_str_eq_packed(name_p, arr_get(x64_ex_name, i))) {
      return i;
    }
    i = (i + 1);
  }
  return (-1);
  return 0;
}

uint8_t x64_find_ext_by_argc(long long name_p, long long argc) {
  long long i;
  i = 0;
  while ((i < arr_len(x64_ex_name))) {
    if (x64_str_eq_packed(name_p, arr_get(x64_ex_name, i))) {
      if ((arr_get(x64_ex_acnt, i) == argc)) {
        return i;
      }
    }
    i = (i + 1);
  }
  return x64_find_ext(name_p);
  return 0;
}

uint8_t x64_arg_reg(long long i) {
  if ((i == 0)) {
    return 7;
  }
  if ((i == 1)) {
    return 6;
  }
  if ((i == 2)) {
    return 2;
  }
  if ((i == 3)) {
    return 1;
  }
  if ((i == 4)) {
    return 8;
  }
  if ((i == 5)) {
    return 9;
  }
  return 0;
  return 0;
}

uint8_t x64_emit_push(void) {
  x64_enc_push(0);
  x64_depth = (x64_depth + 1);
  return 0;
  return 0;
}

uint8_t x64_emit_pop(long long r) {
  x64_enc_pop(r);
  x64_depth = (x64_depth - 1);
  return 0;
  return 0;
}

uint8_t x64_emit_fpush(void) {
  x64_enc_sub_imm(4, 8);
  x64_enc_store_sd(0, 4, 0);
  x64_depth = (x64_depth + 1);
  return 0;
  return 0;
}

uint8_t x64_emit_fpop(long long xr) {
  x64_enc_load_sd(xr, 4, 0);
  x64_enc_add_imm(4, 8);
  x64_depth = (x64_depth - 1);
  return 0;
  return 0;
}

uint8_t x64_align_call(void) {
  if (((x64_depth % 2) != 0)) {
    x64_enc_sub_imm(4, 8);
    return 1;
  }
  return 0;
  return 0;
}

uint8_t x64_unalign_call(long long padded) {
  if (padded) {
    x64_enc_add_imm(4, 8);
  }
  return 0;
  return 0;
}

uint8_t x64_emit_node(long long nd) {
  long long n;
  long long t, op_s, op_l, c0, c1, arr, cnt, i, off, lbl, lbl2, vi, lty, rty;
  long long lit_s, lit_l, sid, str_p, fid, use_flt, is_io, uty, ety, fidx_c, fi, fn_name, par_flt;
  long long padded;
  if ((nd == 0)) {
    x64_enc_xor_zero(0);
    return 0;
  }
  n = nd;
  t = (*(long long*)((uintptr_t)((n + 0))));
  if ((t == T_LIT)) {
    lit_s = unpack_s((*(long long*)((uintptr_t)((n + 8)))));
    lit_l = unpack_l((*(long long*)((uintptr_t)((n + 8)))));
    if ((((long long)(*(uint8_t*)((uintptr_t)((x64_sbuf + lit_s))))) == 34)) {
      sid = arr_len(x64_str_tbl);
      str_p = pack((lit_s + 1), (lit_l - 2));
      x64_str_tbl = arr_push(x64_str_tbl, str_p);
      long long dpos;
      dpos = x64_enc_lea_rip(0);
      x64_enc_add_reloc(dpos, sid, 1);
      return 0;
    }
    if ((x64_is_float_lit((*(long long*)((uintptr_t)((n + 8))))) == 1)) {
      fid = arr_len(x64_flt_tbl);
      x64_flt_tbl = arr_push(x64_flt_tbl, (*(long long*)((uintptr_t)((n + 8)))));
      long long dpos;
      dpos = x64_enc_load_sd_rip(0);
      x64_enc_add_reloc(dpos, fid, 2);
      return 1;
    }
    x64_enc_mov_wide(0, x64_parse_int((*(long long*)((uintptr_t)((n + 8))))));
    return 0;
  }
  if ((t == T_VAR)) {
    vi = x64_var_idx((*(long long*)((uintptr_t)((n + 8)))));
    if ((vi >= 0)) {
      off = arr_get(x64_var_off, vi);
      if (arr_get(x64_var_stack, vi)) {
        x64_enc_lea(0, 5, off);
        return 0;
      }
      return x64_emit_load_var(vi, off);
    }
    if ((ty_get_global_type((*(long long*)((uintptr_t)((n + 8))))) == TY_FLOAT)) {
      long long dpos;
      dpos = x64_enc_lea_rip(10);
      x64_enc_add_reloc(dpos, (*(long long*)((uintptr_t)((n + 8)))), 0);
      x64_enc_load_sd(0, 10, 0);
      return 1;
    }
    long long dpos;
    dpos = x64_enc_lea_rip(10);
    x64_enc_add_reloc(dpos, (*(long long*)((uintptr_t)((n + 8)))), 0);
    x64_enc_load(0, 10, 0);
    return 0;
  }
  if ((t == T_BINOP)) {
    op_s = unpack_s((*(long long*)((uintptr_t)((n + 8)))));
    op_l = unpack_l((*(long long*)((uintptr_t)((n + 8)))));
    c0 = ((long long)(*(uint8_t*)((uintptr_t)((x64_sbuf + op_s)))));
    c1 = 0;
    if ((op_l > 1)) {
      c1 = ((long long)(*(uint8_t*)((uintptr_t)(((x64_sbuf + op_s) + 1)))));
    }
    is_io = 0;
    if ((c0 == 37)) {
      is_io = 1;
    }
    if ((c0 == 94)) {
      is_io = 1;
    }
    if ((c0 == 38)) {
      if ((op_l == 1)) {
        is_io = 1;
      }
    }
    if ((c0 == 124)) {
      if ((op_l == 1)) {
        is_io = 1;
      }
    }
    if ((c0 == 60)) {
      if ((c1 == 60)) {
        is_io = 1;
      }
    }
    if ((c0 == 62)) {
      if ((c1 == 62)) {
        is_io = 1;
      }
    }
    lty = x64_emit_node((*(long long*)((uintptr_t)((n + 16)))));
    if ((lty == 1)) {
      x64_emit_fpush();
    } else {
      x64_emit_push();
    }
    rty = x64_emit_node((*(long long*)((uintptr_t)((n + 24)))));
    use_flt = 0;
    if ((is_io == 0)) {
      if (((lty == 1) | (rty == 1))) {
        use_flt = 1;
      }
    }
    if ((use_flt == 1)) {
      if ((rty == 0)) {
        x64_enc_cvtsi2sd(0, 0);
      }
      if ((lty == 1)) {
        x64_emit_fpop(1);
      } else {
        x64_emit_pop(0);
        x64_enc_cvtsi2sd(1, 0);
      }
      if ((c0 == 43)) {
        if ((op_l == 1)) {
          x64_enc_addsd(0, 1);
          return 1;
        }
      }
      if ((c0 == 45)) {
        if ((op_l == 1)) {
          x64_enc_subsd(1, 0);
          x64_enc_movsd_reg(0, 1);
          return 1;
        }
      }
      if ((c0 == 42)) {
        if ((op_l == 1)) {
          x64_enc_mulsd(0, 1);
          return 1;
        }
      }
      if ((c0 == 47)) {
        if ((op_l == 1)) {
          x64_enc_divsd(1, 0);
          x64_enc_movsd_reg(0, 1);
          return 1;
        }
      }
      if ((c0 == 61)) {
        if ((c1 == 61)) {
          x64_enc_ucomisd(1, 0);
          x64_enc_setcc(CC_E, 0);
          x64_enc_movzx8(0, 0);
          return 0;
        }
      }
      if ((c0 == 33)) {
        if ((c1 == 61)) {
          x64_enc_ucomisd(1, 0);
          x64_enc_setcc(CC_NE, 0);
          x64_enc_movzx8(0, 0);
          return 0;
        }
      }
      if ((c0 == 60)) {
        if ((c1 == 61)) {
          x64_enc_ucomisd(1, 0);
          x64_enc_setcc(CC_BE, 0);
          x64_enc_movzx8(0, 0);
          return 0;
        }
        if ((op_l == 1)) {
          x64_enc_ucomisd(1, 0);
          x64_enc_setcc(CC_B, 0);
          x64_enc_movzx8(0, 0);
          return 0;
        }
      }
      if ((c0 == 62)) {
        if ((c1 == 61)) {
          x64_enc_ucomisd(1, 0);
          x64_enc_setcc(CC_AE, 0);
          x64_enc_movzx8(0, 0);
          return 0;
        }
        if ((op_l == 1)) {
          x64_enc_ucomisd(1, 0);
          x64_enc_setcc(CC_A, 0);
          x64_enc_movzx8(0, 0);
          return 0;
        }
      }
      if ((c0 == 38)) {
        if ((c1 == 38)) {
          x64_enc_xorpd(2, 2);
          x64_enc_ucomisd(1, 2);
          x64_enc_setcc(CC_NE, 0);
          x64_enc_movzx8(0, 0);
          x64_enc_ucomisd(0, 2);
          x64_enc_xorpd(2, 2);
          x64_enc_ucomisd(1, 2);
          x64_enc_setcc(CC_NE, 10);
          x64_enc_movzx8(10, 10);
          x64_enc_ucomisd(0, 2);
          x64_enc_setcc(CC_NE, 0);
          x64_enc_movzx8(0, 0);
          x64_enc_and_reg(0, 10);
          return 0;
        }
      }
      if ((c0 == 124)) {
        if ((c1 == 124)) {
          x64_enc_xorpd(2, 2);
          x64_enc_ucomisd(1, 2);
          x64_enc_setcc(CC_NE, 10);
          x64_enc_movzx8(10, 10);
          x64_enc_ucomisd(0, 2);
          x64_enc_setcc(CC_NE, 0);
          x64_enc_movzx8(0, 0);
          x64_enc_or_reg(0, 10);
          return 0;
        }
      }
      return 1;
    }
    if ((rty == 1)) {
      x64_enc_cvttsd2si(0, 0);
    }
    if ((lty == 1)) {
      x64_enc_mov_reg(1, 0);
      x64_emit_fpop(0);
      x64_enc_cvttsd2si(0, 0);
    } else {
      x64_enc_mov_reg(1, 0);
      x64_emit_pop(0);
    }
    if ((c0 == 43)) {
      if ((op_l == 1)) {
        x64_enc_add_reg(0, 1);
        return 0;
      }
    }
    if ((c0 == 45)) {
      if ((op_l == 1)) {
        x64_enc_sub_reg(0, 1);
        return 0;
      }
    }
    if ((c0 == 42)) {
      if ((op_l == 1)) {
        x64_enc_imul(0, 1);
        return 0;
      }
    }
    if ((c0 == 47)) {
      if ((op_l == 1)) {
        x64_enc_cqo();
        x64_enc_idiv(1);
        return 0;
      }
    }
    if ((c0 == 37)) {
      if ((op_l == 1)) {
        x64_enc_cqo();
        x64_enc_idiv(1);
        x64_enc_mov_reg(0, 2);
        return 0;
      }
    }
    if ((c0 == 38)) {
      if ((op_l == 1)) {
        x64_enc_and_reg(0, 1);
        return 0;
      }
    }
    if ((c0 == 124)) {
      if ((op_l == 1)) {
        x64_enc_or_reg(0, 1);
        return 0;
      }
    }
    if ((c0 == 94)) {
      if ((op_l == 1)) {
        x64_enc_xor_reg(0, 1);
        return 0;
      }
    }
    if ((c0 == 62)) {
      if ((c1 == 62)) {
        x64_enc_sar_cl(0);
        return 0;
      }
    }
    if ((c0 == 60)) {
      if ((c1 == 60)) {
        x64_enc_shl_cl(0);
        return 0;
      }
    }
    if ((c0 == 61)) {
      if ((c1 == 61)) {
        x64_enc_cmp_reg(0, 1);
        x64_enc_setcc(CC_E, 0);
        x64_enc_movzx8(0, 0);
        return 0;
      }
    }
    if ((c0 == 33)) {
      if ((c1 == 61)) {
        x64_enc_cmp_reg(0, 1);
        x64_enc_setcc(CC_NE, 0);
        x64_enc_movzx8(0, 0);
        return 0;
      }
    }
    if ((c0 == 60)) {
      if ((c1 == 61)) {
        x64_enc_cmp_reg(0, 1);
        x64_enc_setcc(CC_LE, 0);
        x64_enc_movzx8(0, 0);
        return 0;
      }
      if ((op_l == 1)) {
        x64_enc_cmp_reg(0, 1);
        x64_enc_setcc(CC_L, 0);
        x64_enc_movzx8(0, 0);
        return 0;
      }
    }
    if ((c0 == 62)) {
      if ((c1 == 61)) {
        x64_enc_cmp_reg(0, 1);
        x64_enc_setcc(CC_GE, 0);
        x64_enc_movzx8(0, 0);
        return 0;
      }
      if ((op_l == 1)) {
        x64_enc_cmp_reg(0, 1);
        x64_enc_setcc(CC_G, 0);
        x64_enc_movzx8(0, 0);
        return 0;
      }
    }
    if ((c0 == 38)) {
      if ((c1 == 38)) {
        x64_enc_test_reg(0, 0);
        x64_enc_setcc(CC_NE, 0);
        x64_enc_movzx8(0, 0);
        x64_enc_test_reg(1, 1);
        x64_enc_setcc(CC_NE, 1);
        x64_enc_movzx8(1, 1);
        x64_enc_and_reg(0, 1);
        return 0;
      }
    }
    if ((c0 == 124)) {
      if ((c1 == 124)) {
        x64_enc_test_reg(0, 0);
        x64_enc_setcc(CC_NE, 0);
        x64_enc_movzx8(0, 0);
        x64_enc_test_reg(1, 1);
        x64_enc_setcc(CC_NE, 1);
        x64_enc_movzx8(1, 1);
        x64_enc_or_reg(0, 1);
        return 0;
      }
    }
    return 0;
  }
  if ((t == T_UNARY)) {
    uty = x64_emit_node((*(long long*)((uintptr_t)((n + 16)))));
    op_s = unpack_s((*(long long*)((uintptr_t)((n + 8)))));
    op_l = unpack_l((*(long long*)((uintptr_t)((n + 8)))));
    if ((op_l == 1)) {
      if ((((long long)(*(uint8_t*)((uintptr_t)((x64_sbuf + op_s))))) == 126)) {
        x64_enc_not(0);
        return 0;
      }
    }
    if ((uty == 1)) {
      x64_enc_xorpd(1, 1);
      x64_enc_subsd(1, 0);
      x64_enc_movsd_reg(0, 1);
      return 1;
    }
    x64_enc_neg(0);
    return 0;
  }
  if ((t == T_CALL)) {
    if (x64_str_eq((*(long long*)((uintptr_t)((n + 8)))), (long long)"putchar", 7)) {
      arr = (*(long long*)((uintptr_t)((n + 16))));
      ety = x64_emit_node((*(long long*)((uintptr_t)((arr + 0)))));
      if ((ety == 1)) {
        x64_enc_cvttsd2si(0, 0);
      }
      x64_enc_storeb(0, 5, (0 - 8));
      x64_enc_mov_wide(0, 1);
      x64_enc_mov_wide(7, 1);
      x64_enc_lea(6, 5, (0 - 8));
      x64_enc_mov_wide(2, 1);
      x64_enc_syscall();
      return 0;
    }
    if (x64_str_eq((*(long long*)((uintptr_t)((n + 8)))), (long long)"getchar", 7)) {
      lbl = x64_enc_new_label();
      lbl2 = x64_enc_new_label();
      x64_enc_xor_zero(10);
      x64_enc_storeb(10, 5, (0 - 8));
      x64_enc_xor_zero(0);
      x64_enc_xor_zero(7);
      x64_enc_lea(6, 5, (0 - 8));
      x64_enc_mov_wide(2, 1);
      x64_enc_syscall();
      x64_enc_cmp_imm(0, 1);
      x64_enc_jcc_label(CC_L, lbl);
      x64_enc_loadb(0, 5, (0 - 8));
      x64_enc_jmp_label(lbl2);
      x64_enc_def_label(lbl);
      x64_enc_mov_wide(0, (-1));
      x64_enc_def_label(lbl2);
      return 0;
    }
    if (x64_str_eq((*(long long*)((uintptr_t)((n + 8)))), (long long)"malloc", 6)) {
      arr = (*(long long*)((uintptr_t)((n + 16))));
      ety = x64_emit_node((*(long long*)((uintptr_t)((arr + 0)))));
      if ((ety == 1)) {
        x64_enc_cvttsd2si(0, 0);
      }
      x64_enc_mov_reg(6, 0);
      x64_enc_xor_zero(7);
      x64_enc_mov_wide(2, 3);
      x64_enc_mov_wide(10, 0x22);
      x64_enc_mov_wide(8, (-1));
      x64_enc_xor_zero(9);
      x64_enc_mov_wide(0, 9);
      x64_enc_syscall();
      return 0;
    }
    if (x64_str_eq((*(long long*)((uintptr_t)((n + 8)))), (long long)"free", 4)) {
      arr = (*(long long*)((uintptr_t)((n + 16))));
      x64_emit_node((*(long long*)((uintptr_t)((arr + 0)))));
      x64_enc_xor_zero(0);
      return 0;
    }
    if (x64_str_eq((*(long long*)((uintptr_t)((n + 8)))), (long long)"realloc", 7)) {
      arr = (*(long long*)((uintptr_t)((n + 16))));
      x64_emit_node((*(long long*)((uintptr_t)((arr + 0)))));
      x64_emit_push();
      x64_emit_node((*(long long*)((uintptr_t)((arr + 8)))));
      x64_emit_push();
      x64_emit_node((*(long long*)((uintptr_t)((arr + 16)))));
      long long lbl_ok, lbl_copy, lbl_end;
      lbl_ok = x64_enc_new_label();
      lbl_copy = x64_enc_new_label();
      lbl_end = x64_enc_new_label();
      x64_enc_mov_reg(2, 0);
      x64_emit_pop(6);
      x64_emit_pop(7);
      x64_enc_mov_wide(10, 1);
      x64_enc_mov_wide(0, 25);
      x64_enc_syscall();
      x64_enc_test_reg(0, 0);
      x64_enc_jcc_label(13, lbl_ok);
      x64_emit_push();
      x64_enc_mov_reg(0, 7);
      x64_emit_push();
      x64_enc_mov_reg(0, 6);
      x64_emit_push();
      x64_enc_mov_reg(6, 2);
      x64_enc_xor_zero(7);
      x64_enc_mov_wide(2, 3);
      x64_enc_mov_wide(10, 0x22);
      x64_enc_mov_wide(8, (-1));
      x64_enc_xor_zero(9);
      x64_enc_mov_wide(0, 9);
      x64_enc_syscall();
      x64_enc_mov_reg(7, 0);
      x64_emit_pop(1);
      x64_emit_pop(6);
      x64_emit_pop(3);
      x64_enc_def_label(lbl_copy);
      x64_enc_test_reg(1, 1);
      x64_enc_jcc_label(4, lbl_end);
      x64_enc_loadb(0, 6, 0);
      x64_enc_storeb(0, 7, 0);
      x64_enc_add_imm(6, 1);
      x64_enc_add_imm(7, 1);
      x64_enc_sub_imm(1, 1);
      x64_enc_jcc_label(5, lbl_copy);
      x64_enc_mov_reg(0, 7);
      x64_enc_jmp_label(lbl_end);
      x64_enc_def_label(lbl_ok);
      x64_enc_def_label(lbl_end);
      return 0;
    }
    if (x64_str_eq((*(long long*)((uintptr_t)((n + 8)))), (long long)"memcpy", 6)) {
      arr = (*(long long*)((uintptr_t)((n + 16))));
      x64_emit_node((*(long long*)((uintptr_t)((arr + 0)))));
      x64_emit_push();
      x64_emit_node((*(long long*)((uintptr_t)((arr + 8)))));
      x64_emit_push();
      x64_emit_node((*(long long*)((uintptr_t)((arr + 16)))));
      long long lbl_top, lbl_end;
      x64_enc_mov_reg(1, 0);
      x64_emit_pop(6);
      x64_emit_pop(7);
      lbl_top = x64_enc_new_label();
      lbl_end = x64_enc_new_label();
      x64_enc_test_reg(1, 1);
      x64_enc_jcc_label(4, lbl_end);
      x64_enc_def_label(lbl_top);
      x64_enc_loadb(0, 6, 0);
      x64_enc_storeb(0, 7, 0);
      x64_enc_add_imm(6, 1);
      x64_enc_add_imm(7, 1);
      x64_enc_sub_imm(1, 1);
      x64_enc_jcc_label(5, lbl_top);
      x64_enc_def_label(lbl_end);
      x64_enc_mov_reg(0, 7);
      return 0;
    }
    if (x64_str_eq((*(long long*)((uintptr_t)((n + 8)))), (long long)"peek", 4)) {
      arr = (*(long long*)((uintptr_t)((n + 16))));
      ety = x64_emit_node((*(long long*)((uintptr_t)((arr + 0)))));
      if ((ety == 1)) {
        x64_enc_cvttsd2si(0, 0);
      }
      x64_enc_loadb(0, 0, 0);
      return 0;
    }
    if (x64_str_eq((*(long long*)((uintptr_t)((n + 8)))), (long long)"poke", 4)) {
      arr = (*(long long*)((uintptr_t)((n + 16))));
      ety = x64_emit_node((*(long long*)((uintptr_t)((arr + 8)))));
      if ((ety == 1)) {
        x64_enc_cvttsd2si(0, 0);
      }
      x64_emit_push();
      ety = x64_emit_node((*(long long*)((uintptr_t)((arr + 0)))));
      if ((ety == 1)) {
        x64_enc_cvttsd2si(0, 0);
      }
      x64_emit_pop(1);
      x64_enc_storeb(1, 0, 0);
      return 0;
    }
    if (x64_str_eq((*(long long*)((uintptr_t)((n + 8)))), (long long)"float_ret2", 10)) {
      x64_enc_movsd_reg(0, 1);
      return 1;
    }
    if (x64_str_eq((*(long long*)((uintptr_t)((n + 8)))), (long long)"str_peek", 8)) {
      arr = (*(long long*)((uintptr_t)((n + 16))));
      ety = x64_emit_node((*(long long*)((uintptr_t)((arr + 8)))));
      if ((ety == 1)) {
        x64_enc_cvttsd2si(0, 0);
      }
      x64_emit_push();
      ety = x64_emit_node((*(long long*)((uintptr_t)((arr + 0)))));
      if ((ety == 1)) {
        x64_enc_cvttsd2si(0, 0);
      }
      x64_emit_pop(1);
      x64_enc_add_reg(0, 1);
      x64_enc_loadb(0, 0, 0);
      return 0;
    }
    if (x64_str_eq((*(long long*)((uintptr_t)((n + 8)))), (long long)"sys_write", 9)) {
      arr = (*(long long*)((uintptr_t)((n + 16))));
      x64_emit_node((*(long long*)((uintptr_t)((arr + 16)))));
      x64_emit_push();
      x64_emit_node((*(long long*)((uintptr_t)((arr + 8)))));
      x64_emit_push();
      x64_emit_node((*(long long*)((uintptr_t)((arr + 0)))));
      x64_enc_mov_reg(7, 0);
      x64_emit_pop(6);
      x64_emit_pop(2);
      x64_enc_mov_wide(0, 1);
      x64_enc_syscall();
      return 0;
    }
    if (x64_str_eq((*(long long*)((uintptr_t)((n + 8)))), (long long)"sys_read", 8)) {
      arr = (*(long long*)((uintptr_t)((n + 16))));
      x64_emit_node((*(long long*)((uintptr_t)((arr + 16)))));
      x64_emit_push();
      x64_emit_node((*(long long*)((uintptr_t)((arr + 8)))));
      x64_emit_push();
      x64_emit_node((*(long long*)((uintptr_t)((arr + 0)))));
      x64_enc_mov_reg(7, 0);
      x64_emit_pop(6);
      x64_emit_pop(2);
      x64_enc_xor_zero(0);
      x64_enc_syscall();
      return 0;
    }
    if (x64_str_eq((*(long long*)((uintptr_t)((n + 8)))), (long long)"sys_open", 8)) {
      arr = (*(long long*)((uintptr_t)((n + 16))));
      x64_emit_node((*(long long*)((uintptr_t)((arr + 8)))));
      x64_emit_push();
      x64_emit_node((*(long long*)((uintptr_t)((arr + 0)))));
      x64_enc_mov_reg(7, 0);
      x64_emit_pop(6);
      x64_enc_mov_wide(2, 493);
      x64_enc_mov_wide(0, 2);
      x64_enc_syscall();
      return 0;
    }
    if (x64_str_eq((*(long long*)((uintptr_t)((n + 8)))), (long long)"sys_close", 9)) {
      arr = (*(long long*)((uintptr_t)((n + 16))));
      x64_emit_node((*(long long*)((uintptr_t)((arr + 0)))));
      x64_enc_mov_reg(7, 0);
      x64_enc_mov_wide(0, 3);
      x64_enc_syscall();
      return 0;
    }
    if (x64_str_eq((*(long long*)((uintptr_t)((n + 8)))), (long long)"sys_exit", 8)) {
      arr = (*(long long*)((uintptr_t)((n + 16))));
      x64_emit_node((*(long long*)((uintptr_t)((arr + 0)))));
      x64_enc_mov_reg(7, 0);
      x64_enc_mov_wide(0, 60);
      x64_enc_syscall();
      return 0;
    }
    if (x64_str_eq((*(long long*)((uintptr_t)((n + 8)))), (long long)"sys_fork", 8)) {
      x64_enc_mov_wide(0, 57);
      x64_enc_syscall();
      return 0;
    }
    if (x64_str_eq((*(long long*)((uintptr_t)((n + 8)))), (long long)"sys_execve", 10)) {
      arr = (*(long long*)((uintptr_t)((n + 16))));
      x64_emit_node((*(long long*)((uintptr_t)((arr + 0)))));
      x64_emit_push();
      x64_emit_node((*(long long*)((uintptr_t)((arr + 8)))));
      x64_emit_push();
      x64_emit_node((*(long long*)((uintptr_t)((arr + 16)))));
      x64_enc_mov_reg(2, 0);
      x64_emit_pop(6);
      x64_emit_pop(7);
      x64_enc_mov_wide(0, 59);
      x64_enc_syscall();
      return 0;
    }
    if (x64_str_eq((*(long long*)((uintptr_t)((n + 8)))), (long long)"sys_waitpid", 11)) {
      arr = (*(long long*)((uintptr_t)((n + 16))));
      x64_emit_node((*(long long*)((uintptr_t)((arr + 0)))));
      x64_enc_mov_reg(7, 0);
      x64_enc_xor_zero(6);
      x64_enc_xor_zero(2);
      x64_enc_xor_zero(10);
      x64_enc_mov_wide(0, 61);
      x64_enc_syscall();
      return 0;
    }
    if (x64_str_eq((*(long long*)((uintptr_t)((n + 8)))), (long long)"sys_ioctl", 9)) {
      arr = (*(long long*)((uintptr_t)((n + 16))));
      x64_emit_node((*(long long*)((uintptr_t)((arr + 16)))));
      x64_emit_push();
      x64_emit_node((*(long long*)((uintptr_t)((arr + 8)))));
      x64_emit_push();
      x64_emit_node((*(long long*)((uintptr_t)((arr + 0)))));
      x64_enc_mov_reg(7, 0);
      x64_emit_pop(6);
      x64_emit_pop(2);
      x64_enc_mov_wide(0, 16);
      x64_enc_syscall();
      return 0;
    }
    if (x64_str_eq((*(long long*)((uintptr_t)((n + 8)))), (long long)"sys_nanosleep", 13)) {
      arr = (*(long long*)((uintptr_t)((n + 16))));
      x64_emit_node((*(long long*)((uintptr_t)((arr + 8)))));
      x64_emit_push();
      x64_emit_node((*(long long*)((uintptr_t)((arr + 0)))));
      x64_enc_mov_reg(7, 0);
      x64_emit_pop(6);
      x64_enc_mov_wide(0, 35);
      x64_enc_syscall();
      return 0;
    }
    if (x64_str_eq((*(long long*)((uintptr_t)((n + 8)))), (long long)"sys_clock_gettime", 17)) {
      arr = (*(long long*)((uintptr_t)((n + 16))));
      x64_emit_node((*(long long*)((uintptr_t)((arr + 8)))));
      x64_emit_push();
      x64_emit_node((*(long long*)((uintptr_t)((arr + 0)))));
      x64_enc_mov_reg(7, 0);
      x64_emit_pop(6);
      x64_enc_mov_wide(0, 228);
      x64_enc_syscall();
      return 0;
    }
    if (x64_str_eq((*(long long*)((uintptr_t)((n + 8)))), (long long)"argc_get", 8)) {
      long long dpos;
      dpos = x64_enc_lea_rip(10);
      x64_enc_add_reloc(dpos, x64_argc_sym, 0);
      x64_enc_load(0, 10, 0);
      return 0;
    }
    if (x64_str_eq((*(long long*)((uintptr_t)((n + 8)))), (long long)"argv_get", 8)) {
      x64_emit_node((*(long long*)((uintptr_t)((*(long long*)((uintptr_t)((n + 16))))))));
      x64_emit_push();
      long long dpos;
      dpos = x64_enc_lea_rip(10);
      x64_enc_add_reloc(dpos, x64_argv_sym, 0);
      x64_enc_load(0, 10, 0);
      x64_emit_pop(1);
      x64_enc_shl_imm(1, 3);
      x64_enc_add_reg(0, 1);
      x64_enc_load(0, 0, 0);
      return 0;
    }
    if (x64_str_eq((*(long long*)((uintptr_t)((n + 8)))), (long long)"fn_ptr", 6)) {
      arr = (*(long long*)((uintptr_t)((n + 16))));
      fn_name = (*(long long*)((uintptr_t)((arr + 0))));
      fi = x64_find_fn((*(long long*)((uintptr_t)((fn_name + 8)))));
      if ((fi < 0)) {
        return 0;
      }
      long long dpos;
      dpos = x64_enc_lea_rip(0);
      x64_enc_add_fixup(arr_get(x64_fn_lbl, fi));
      arr_set(x64_enc_fix_pos, (arr_len(x64_enc_fix_pos) - 1), dpos);
      return 0;
    }
    if (x64_str_eq((*(long long*)((uintptr_t)((n + 8)))), (long long)"call", 4)) {
      arr = (*(long long*)((uintptr_t)((n + 16))));
      cnt = (*(long long*)((uintptr_t)((n + 24))));
      i = 0;
      while ((i < cnt)) {
        ety = x64_emit_node((*(long long*)((uintptr_t)((arr + (i * 8))))));
        if ((ety == 1)) {
          x64_enc_movq_from_xmm(0, 0);
        }
        x64_emit_push();
        i = (i + 1);
      }
      i = (cnt - 2);
      while ((i >= 0)) {
        x64_emit_pop(x64_arg_reg(i));
        i = (i - 1);
      }
      x64_emit_pop(11);
      padded = x64_align_call();
      x64_enc_call_reg(11);
      x64_unalign_call(padded);
      return 0;
    }
    fidx_c = find_func_idx((*(long long*)((uintptr_t)((n + 8)))));
    arr = (*(long long*)((uintptr_t)((n + 16))));
    cnt = (*(long long*)((uintptr_t)((n + 24))));
    i = 0;
    while ((i < cnt)) {
      ety = x64_emit_node((*(long long*)((uintptr_t)((arr + (i * 8))))));
      if ((fidx_c >= 0)) {
        par_flt = (get_fn_par_type(fidx_c, i) == TY_FLOAT);
        if ((ety == 1)) {
          if (par_flt) {
            x64_enc_movq_from_xmm(0, 0);
          } else {
            x64_enc_cvttsd2si(0, 0);
          }
        } else {
          if (par_flt) {
            x64_enc_cvtsi2sd(0, 0);
            x64_enc_movq_from_xmm(0, 0);
          }
        }
      } else {
        if ((ety == 1)) {
          x64_enc_movq_from_xmm(0, 0);
        }
      }
      x64_emit_push();
      i = (i + 1);
    }
    i = (cnt - 1);
    while ((i >= 0)) {
      x64_emit_pop(x64_arg_reg(i));
      i = (i - 1);
    }
    fi = x64_find_fn((*(long long*)((uintptr_t)((n + 8)))));
    padded = x64_align_call();
    if ((fi >= 0)) {
      x64_enc_call_label(arr_get(x64_fn_lbl, fi));
    } else {
      x64_enc_add_reloc((x64_enc_pos + 1), (*(long long*)((uintptr_t)((n + 8)))), 4);
      x64_enc_emit8(0xE8);
      x64_enc_emit32(0);
    }
    x64_unalign_call(padded);
    if ((fidx_c >= 0)) {
      if ((get_fn_ret_type(fidx_c) == TY_FLOAT)) {
        return 1;
      }
    }
    return 0;
  }
  if ((t == T_MEMLD)) {
    ety = x64_emit_node((*(long long*)((uintptr_t)((n + 8)))));
    if ((ety == 1)) {
      x64_enc_cvttsd2si(0, 0);
    }
    x64_enc_load(0, 0, 0);
    return 0;
  }
  return 0;
  return 0;
}

uint8_t x64_emit_body(long long arr, long long cnt) {
  long long i;
  long long n;
  i = 0;
  while ((i < cnt)) {
    n = (*(long long*)((uintptr_t)((arr + (i * 8)))));
    if ((n != 0)) {
      x64_emit_stmt(n);
    }
    i = (i + 1);
  }
  return 0;
  return 0;
}

uint8_t x64_emit_stmt(long long nd) {
  long long n;
  long long lhs;
  long long t, off, lbl, lbl2, arr, cnt, i, rty, vi, ety;
  if ((nd == 0)) {
    return 0;
  }
  n = nd;
  t = (*(long long*)((uintptr_t)((n + 0))));
  if ((t == T_DECL)) {
    return 0;
  }
  if ((t == T_ASSIGN)) {
    lhs = (*(long long*)((uintptr_t)((n + 8))));
    if (((*(long long*)((uintptr_t)((lhs + 0)))) == T_VAR)) {
      vi = x64_var_idx((*(long long*)((uintptr_t)((lhs + 8)))));
      if ((vi >= 0)) {
        if (arr_get(x64_var_stack, vi)) {
          x64_emit_node((*(long long*)((uintptr_t)((n + 16)))));
          off = arr_get(x64_var_off, vi);
          long long fc, j, sidx;
          sidx = arr_get(x64_var_struct_idx, vi);
          fc = (get_struct_size(sidx) / 8);
          j = 0;
          while ((j < fc)) {
            x64_enc_load(10, 0, (j * 8));
            x64_enc_store(10, 5, (off + (j * 8)));
            j = (j + 1);
          }
          return 0;
        }
      }
    }
    rty = x64_emit_node((*(long long*)((uintptr_t)((n + 16)))));
    if (((*(long long*)((uintptr_t)((lhs + 0)))) == T_VAR)) {
      vi = x64_var_idx((*(long long*)((uintptr_t)((lhs + 8)))));
      if ((vi >= 0)) {
        off = arr_get(x64_var_off, vi);
        x64_emit_store_var(vi, off, rty);
      } else {
        long long dpos;
        if ((ty_get_global_type((*(long long*)((uintptr_t)((lhs + 8))))) == TY_FLOAT)) {
          if ((rty == 0)) {
            x64_enc_cvtsi2sd(0, 0);
          }
          dpos = x64_enc_lea_rip(10);
          x64_enc_add_reloc(dpos, (*(long long*)((uintptr_t)((lhs + 8)))), 0);
          x64_enc_store_sd(0, 10, 0);
        } else {
          if ((rty == 1)) {
            x64_enc_cvttsd2si(0, 0);
          }
          dpos = x64_enc_lea_rip(10);
          x64_enc_add_reloc(dpos, (*(long long*)((uintptr_t)((lhs + 8)))), 0);
          x64_enc_store(0, 10, 0);
        }
      }
    }
    return 0;
  }
  if ((t == T_MEMST)) {
    ety = x64_emit_node((*(long long*)((uintptr_t)((n + 16)))));
    if ((ety == 1)) {
      x64_enc_movq_from_xmm(0, 0);
    }
    x64_emit_push();
    x64_emit_node((*(long long*)((uintptr_t)((n + 8)))));
    x64_emit_pop(1);
    x64_enc_store(1, 0, 0);
    return 0;
  }
  if ((t == T_IF)) {
    lbl = x64_enc_new_label();
    lbl2 = x64_enc_new_label();
    ety = x64_emit_node((*(long long*)((uintptr_t)((n + 8)))));
    if ((ety == 1)) {
      x64_enc_xorpd(1, 1);
      x64_enc_ucomisd(0, 1);
      x64_enc_setcc(CC_NE, 0);
      x64_enc_movzx8(0, 0);
    }
    x64_enc_test_reg(0, 0);
    x64_enc_jcc_label(CC_E, lbl);
    x64_emit_body((*(long long*)((uintptr_t)((n + 16)))), (*(long long*)((uintptr_t)((n + 24)))));
    x64_enc_jmp_label(lbl2);
    x64_enc_def_label(lbl);
    if (((*(long long*)((uintptr_t)((n + 40)))) > 0)) {
      x64_emit_body((*(long long*)((uintptr_t)((n + 32)))), (*(long long*)((uintptr_t)((n + 40)))));
    }
    x64_enc_def_label(lbl2);
    return 0;
  }
  if ((t == T_BREAK)) {
    if ((arr_len(x64_break_stack) > 0)) {
      long long brk_lbl;
      brk_lbl = arr_last(x64_break_stack);
      x64_enc_jmp_label(brk_lbl);
    }
    return 0;
  }
  if ((t == T_WHILE)) {
    lbl = x64_enc_new_label();
    lbl2 = x64_enc_new_label();
    x64_break_stack = arr_push(x64_break_stack, lbl2);
    x64_enc_def_label(lbl);
    ety = x64_emit_node((*(long long*)((uintptr_t)((n + 8)))));
    if ((ety == 1)) {
      x64_enc_xorpd(1, 1);
      x64_enc_ucomisd(0, 1);
      x64_enc_setcc(CC_NE, 0);
      x64_enc_movzx8(0, 0);
    }
    x64_enc_test_reg(0, 0);
    x64_enc_jcc_label(CC_E, lbl2);
    x64_emit_body((*(long long*)((uintptr_t)((n + 16)))), (*(long long*)((uintptr_t)((n + 24)))));
    x64_enc_jmp_label(lbl);
    x64_enc_def_label(lbl2);
    arr_pop(x64_break_stack);
    return 0;
  }
  if ((t == T_RET)) {
    rty = x64_emit_node((*(long long*)((uintptr_t)((n + 8)))));
    if ((get_fn_ret_type(x64_cur_fn_idx) == TY_FLOAT)) {
      if ((rty == 0)) {
        x64_enc_cvtsi2sd(0, 0);
      }
    } else {
      if ((rty == 1)) {
        x64_enc_cvttsd2si(0, 0);
      }
    }
    x64_enc_jmp_label(x64_ret_lbl);
    return 0;
  }
  if ((t == T_NOP)) {
    return 0;
  }
  x64_emit_node(nd);
  return 0;
  return 0;
}

uint8_t x64_emit_func(long long fi) {
  long long name_p, par_arr, par_cnt, body_arr, body_cnt;
  long long frame, i, total, sz;
  name_p = arr_get(x64_fn_name, fi);
  par_arr = arr_get(x64_fn_par, fi);
  par_cnt = arr_get(x64_fn_pcnt, fi);
  body_arr = arr_get(x64_fn_body, fi);
  body_cnt = arr_get(x64_fn_bcnt, fi);
  arr_clear(x64_vars);
  arr_clear(x64_var_stack);
  arr_clear(x64_var_struct_idx);
  arr_clear(x64_var_off);
  arr_clear(x64_var_forced_ty);
  x64_cur_fn_name = name_p;
  x64_cur_fn_idx = arr_get(x64_fn_fidx, fi);
  long long fn_rec, hints, ph;
  fn_rec = arr_get(funcs, x64_cur_fn_idx);
  hints = (*(long long*)((uintptr_t)((fn_rec + 48))));
  i = 0;
  while ((i < par_cnt)) {
    long long pvi;
    pvi = x64_var_add((*(long long*)((uintptr_t)((par_arr + (i * 8))))));
    if ((hints != 0)) {
      ph = (*(long long*)((uintptr_t)((hints + (i * 8)))));
      if ((ph > 0)) {
        arr_set(x64_var_forced_ty, pvi, ph);
      }
    }
    i = (i + 1);
  }
  x64_pre_reg_vars(body_arr, body_cnt);
  total = 0;
  i = 0;
  while ((i < arr_len(x64_vars))) {
    if (arr_get(x64_var_stack, i)) {
      sz = get_struct_size(arr_get(x64_var_struct_idx, i));
    } else {
      sz = 8;
    }
    total = (total + sz);
    arr_set(x64_var_off, i, (0 - (8 + total)));
    i = (i + 1);
  }
  frame = (8 + total);
  frame = (((frame + 15) / 16) * 16);
  x64_enc_def_label(arr_get(x64_fn_lbl, fi));
  x64_ret_lbl = x64_enc_new_label();
  x64_enc_push(5);
  x64_enc_mov_reg(5, 4);
  if ((frame > 0)) {
    x64_enc_sub_imm(4, frame);
  }
  if (x64_str_eq(name_p, (long long)"main", 4)) {
    long long dpos;
    dpos = x64_enc_lea_rip(10);
    x64_enc_add_reloc(dpos, x64_argc_sym, 0);
    x64_enc_store(7, 10, 0);
    dpos = x64_enc_lea_rip(10);
    x64_enc_add_reloc(dpos, x64_argv_sym, 0);
    x64_enc_store(6, 10, 0);
  }
  i = 0;
  while ((i < par_cnt)) {
    x64_enc_store(x64_arg_reg(i), 5, arr_get(x64_var_off, i));
    i = (i + 1);
  }
  x64_depth = 0;
  x64_emit_body(body_arr, body_cnt);
  x64_enc_def_label(x64_ret_lbl);
  x64_enc_mov_reg(4, 5);
  x64_enc_pop(5);
  x64_enc_ret();
  return 0;
  return 0;
}

uint8_t x64_mod_init(void) {
  long long i, rec, name_p, idx, j;
  x64_sbuf = vbuf;
  x64_enc_pos = 0;
  arr_clear(x64_enc_lbl_off);
  arr_clear(x64_enc_fix_pos);
  arr_clear(x64_enc_fix_lbl);
  arr_clear(x64_enc_rel_pos);
  arr_clear(x64_enc_rel_sym);
  arr_clear(x64_enc_rel_ty);
  arr_clear(x64_fn_name);
  arr_clear(x64_fn_par);
  arr_clear(x64_fn_pcnt);
  arr_clear(x64_fn_body);
  arr_clear(x64_fn_bcnt);
  arr_clear(x64_fn_fidx);
  arr_clear(x64_fn_lbl);
  arr_clear(x64_gl_name);
  i = 0;
  while ((i < glob_cnt)) {
    name_p = (*(long long*)((uintptr_t)((globals + (i * 8)))));
    idx = (-1);
    j = 0;
    while ((j < arr_len(x64_gl_name))) {
      if (x64_str_eq_packed(name_p, arr_get(x64_gl_name, j))) {
        idx = j;
      }
      j = (j + 1);
    }
    if ((idx < 0)) {
      x64_gl_name = arr_push(x64_gl_name, name_p);
    }
    i = (i + 1);
  }
  x64_gl_name = arr_push(x64_gl_name, x64_argc_sym);
  x64_gl_name = arr_push(x64_gl_name, x64_argv_sym);
  arr_clear(x64_ex_name);
  arr_clear(x64_ex_ret);
  arr_clear(x64_ex_args);
  arr_clear(x64_ex_acnt);
  i = 0;
  while ((i < ext_cnt)) {
    rec = (*(long long*)((uintptr_t)((externs + (i * 8)))));
    x64_ex_name = arr_push(x64_ex_name, (*(long long*)((uintptr_t)((rec + EX_NAME)))));
    x64_ex_ret = arr_push(x64_ex_ret, (*(long long*)((uintptr_t)((rec + EX_RET)))));
    x64_ex_args = arr_push(x64_ex_args, (*(long long*)((uintptr_t)((rec + EX_ARGS)))));
    x64_ex_acnt = arr_push(x64_ex_acnt, (*(long long*)((uintptr_t)((rec + EX_ACNT)))));
    i = (i + 1);
  }
  return 0;
  return 0;
}

long long emit_module_x86_64(long long mi) {
  long long fn_start, code_start, i, rec, name_p, idx, j;
  fn_start = arr_len(x64_fn_name);
  i = 0;
  while ((i < func_cnt)) {
    if ((arr_get(func_mods, i) == mi)) {
      rec = (*(long long*)((uintptr_t)((funcs + (i * 8)))));
      name_p = (*(long long*)((uintptr_t)((rec + FN_NAME))));
      idx = (-1);
      j = 0;
      while ((j < arr_len(x64_fn_name))) {
        if (x64_str_eq_packed(name_p, arr_get(x64_fn_name, j))) {
          idx = j;
        }
        j = (j + 1);
      }
      if ((idx < 0)) {
        idx = arr_len(x64_fn_name);
        x64_fn_name = arr_push(x64_fn_name, name_p);
        x64_fn_par = arr_push(x64_fn_par, (*(long long*)((uintptr_t)((rec + FN_PARS)))));
        x64_fn_pcnt = arr_push(x64_fn_pcnt, (*(long long*)((uintptr_t)((rec + FN_PCNT)))));
        x64_fn_body = arr_push(x64_fn_body, (*(long long*)((uintptr_t)((rec + FN_BODY)))));
        x64_fn_bcnt = arr_push(x64_fn_bcnt, (*(long long*)((uintptr_t)((rec + FN_BCNT)))));
        x64_fn_fidx = arr_push(x64_fn_fidx, i);
      } else {
        arr_set(x64_fn_name, idx, name_p);
        arr_set(x64_fn_par, idx, (*(long long*)((uintptr_t)((rec + FN_PARS)))));
        arr_set(x64_fn_pcnt, idx, (*(long long*)((uintptr_t)((rec + FN_PCNT)))));
        arr_set(x64_fn_body, idx, (*(long long*)((uintptr_t)((rec + FN_BODY)))));
        arr_set(x64_fn_bcnt, idx, (*(long long*)((uintptr_t)((rec + FN_BCNT)))));
        arr_set(x64_fn_fidx, idx, i);
      }
    }
    i = (i + 1);
  }
  i = fn_start;
  while ((i < arr_len(x64_fn_name))) {
    x64_fn_lbl = arr_push(x64_fn_lbl, x64_enc_new_label());
    i = (i + 1);
  }
  arr_clear(x64_enc_fix_pos);
  arr_clear(x64_enc_fix_lbl);
  code_start = x64_enc_pos;
  i = fn_start;
  while ((i < arr_len(x64_fn_name))) {
    x64_emit_func(i);
    i = (i + 1);
  }
  x64_enc_resolve_fixups();
  return code_start;
  return 0;
}

long long x64_emit_start_stub(void) {
  long long start_lbl, main_fi, i;
  arr_clear(x64_enc_fix_pos);
  arr_clear(x64_enc_fix_lbl);
  start_lbl = x64_enc_new_label();
  x64_enc_def_label(start_lbl);
  x64_enc_pop(7);
  x64_enc_mov_reg(6, 4);
  x64_enc_and_imm(4, (0 - 16));
  main_fi = (0 - 1);
  i = 0;
  while ((i < arr_len(x64_fn_name))) {
    if (x64_str_eq(arr_get(x64_fn_name, i), (long long)"main", 4)) {
      main_fi = i;
    }
    i = (i + 1);
  }
  if ((main_fi >= 0)) {
    x64_enc_call_label(arr_get(x64_fn_lbl, main_fi));
  }
  x64_enc_mov_reg(7, 0);
  x64_enc_mov_wide(0, 60);
  x64_enc_syscall();
  x64_enc_resolve_fixups();
  return start_lbl;
  return 0;
}

long long emit_all_x86_64(void) {
  long long i, sp, ss, sl, j, fp, fs, fl, main_fi, main_lbl, start_lbl;
  bridge_data_x86_64();
  x64_gl_name = arr_push(x64_gl_name, x64_argc_sym);
  x64_gl_name = arr_push(x64_gl_name, x64_argv_sym);
  x64_enc_pos = 0;
  arr_clear(x64_enc_lbl_off);
  arr_clear(x64_enc_fix_pos);
  arr_clear(x64_enc_fix_lbl);
  arr_clear(x64_enc_rel_pos);
  arr_clear(x64_enc_rel_sym);
  arr_clear(x64_enc_rel_ty);
  arr_clear(x64_fn_lbl);
  i = 0;
  while ((i < arr_len(x64_fn_name))) {
    x64_fn_lbl = arr_push(x64_fn_lbl, x64_enc_new_label());
    i = (i + 1);
  }
  start_lbl = x64_enc_new_label();
  x64_enc_def_label(start_lbl);
  x64_enc_pop(7);
  x64_enc_mov_reg(6, 4);
  x64_enc_and_imm(4, (0 - 16));
  main_fi = x64_find_fn(pack(0, 0));
  i = 0;
  while ((i < arr_len(x64_fn_name))) {
    if (x64_str_eq(arr_get(x64_fn_name, i), (long long)"main", 4)) {
      main_fi = i;
    }
    i = (i + 1);
  }
  if ((main_fi >= 0)) {
    x64_enc_call_label(arr_get(x64_fn_lbl, main_fi));
  }
  x64_enc_mov_reg(7, 0);
  x64_enc_mov_wide(0, 60);
  x64_enc_syscall();
  i = 0;
  while ((i < arr_len(x64_fn_name))) {
    x64_emit_func(i);
    i = (i + 1);
  }
  x64_enc_resolve_fixups();
  return start_lbl;
  return 0;
}

uint8_t init_elf(void) {
  ELF_PAGE = 4096;
  ELF_BASE = 0x400000;
  ELF_HDRSZ = 64;
  ELF_PHSZ = 56;
  elf_buf = (long long)malloc(16777216);
  elf_pos = 0;
  elf_flt_data = arr_new();
  elf_sdata = (long long)malloc(1048576);
  elf_sdata_size = 0;
  elf_sdata_off = arr_new();
  return 0;
  return 0;
}

uint8_t elf_w8(long long v) {
  (*(uint8_t*)((uintptr_t)((elf_buf + elf_pos))) = (uint8_t)((v & 0xFF)));
  elf_pos = (elf_pos + 1);
  return 0;
  return 0;
}

uint8_t elf_w16(long long v) {
  (*(uint8_t*)((uintptr_t)((elf_buf + elf_pos))) = (uint8_t)((v & 0xFF)));
  (*(uint8_t*)((uintptr_t)(((elf_buf + elf_pos) + 1))) = (uint8_t)(((v >> 8) & 0xFF)));
  elf_pos = (elf_pos + 2);
  return 0;
  return 0;
}

uint8_t elf_w32(long long v) {
  (*(uint8_t*)((uintptr_t)((elf_buf + elf_pos))) = (uint8_t)((v & 0xFF)));
  (*(uint8_t*)((uintptr_t)(((elf_buf + elf_pos) + 1))) = (uint8_t)(((v >> 8) & 0xFF)));
  (*(uint8_t*)((uintptr_t)(((elf_buf + elf_pos) + 2))) = (uint8_t)(((v >> 16) & 0xFF)));
  (*(uint8_t*)((uintptr_t)(((elf_buf + elf_pos) + 3))) = (uint8_t)(((v >> 24) & 0xFF)));
  elf_pos = (elf_pos + 4);
  return 0;
  return 0;
}

uint8_t elf_w64(long long v) {
  elf_w32((v & 0xFFFFFFFF));
  elf_w32(((v >> 32) & 0xFFFFFFFF));
  return 0;
  return 0;
}

uint8_t elf_pad(long long n) {
  long long i;
  i = 0;
  while ((i < n)) {
    (*(uint8_t*)((uintptr_t)((elf_buf + elf_pos))) = (uint8_t)(0));
    elf_pos = (elf_pos + 1);
    i = (i + 1);
  }
  return 0;
  return 0;
}

uint8_t elf_align(long long a) {
  while (((elf_pos % a) != 0)) {
    (*(uint8_t*)((uintptr_t)((elf_buf + elf_pos))) = (uint8_t)(0));
    elf_pos = (elf_pos + 1);
  }
  return 0;
  return 0;
}

uint8_t elf_wcstr(long long s, long long n) {
  long long i;
  i = 0;
  while ((i < n)) {
    (*(uint8_t*)((uintptr_t)((elf_buf + elf_pos))) = (uint8_t)((long long)((uint8_t*)((uintptr_t)(s)))[(i)]));
    elf_pos = (elf_pos + 1);
    i = (i + 1);
  }
  return 0;
  return 0;
}

long long elf_page_align(long long v) {
  return ((((v + ELF_PAGE) - 1) / ELF_PAGE) * ELF_PAGE);
  return 0;
}

uint8_t elf_write_header(void) {
  elf_w8(0x7F);
  elf_w8(0x45);
  elf_w8(0x4C);
  elf_w8(0x46);
  elf_w8(2);
  elf_w8(1);
  elf_w8(1);
  elf_w8(0);
  elf_pad(8);
  elf_w16(2);
  elf_w16(0x3E);
  elf_w32(1);
  elf_w64(elf_entry);
  elf_w64(ELF_HDRSZ);
  elf_w64(0);
  elf_w32(0);
  elf_w16(ELF_HDRSZ);
  elf_w16(ELF_PHSZ);
  elf_w16(elf_phnum);
  elf_w16(0);
  elf_w16(0);
  elf_w16(0);
  return 0;
  return 0;
}

uint8_t elf_write_phdr(long long p_type, long long p_flags, long long p_offset, long long p_vaddr, long long p_filesz, long long p_memsz, long long p_align) {
  elf_w32(p_type);
  elf_w32(p_flags);
  elf_w64(p_offset);
  elf_w64(p_vaddr);
  elf_w64(p_vaddr);
  elf_w64(p_filesz);
  elf_w64(p_memsz);
  elf_w64(p_align);
  return 0;
  return 0;
}

uint8_t elf_compute_layout(long long code_size, long long gl_count) {
  long long raw_data_size;
  elf_has_data = 0;
  if ((gl_count > 0)) {
    elf_has_data = 1;
  }
  if ((arr_len(elf_flt_data) > 0)) {
    elf_has_data = 1;
  }
  if ((elf_sdata_size > 0)) {
    elf_has_data = 1;
  }
  elf_phnum = 1;
  if (elf_has_data) {
    elf_phnum = 2;
  }
  elf_code_off = (ELF_HDRSZ + (elf_phnum * ELF_PHSZ));
  elf_text_filesz = (elf_code_off + code_size);
  elf_text_memsz = elf_text_filesz;
  if (elf_has_data) {
    elf_data_off = elf_page_align(elf_text_filesz);
    elf_data_vaddr = (ELF_BASE + elf_data_off);
    raw_data_size = (((gl_count * 8) + (arr_len(elf_flt_data) * 8)) + elf_sdata_size);
    elf_data_filesz = raw_data_size;
    elf_data_memsz = raw_data_size;
  } else {
    elf_data_off = 0;
    elf_data_vaddr = 0;
    elf_data_filesz = 0;
    elf_data_memsz = 0;
  }
  return 0;
  return 0;
}

uint8_t elf_resolve_relocations(long long gl_names, long long gl_count) {
  long long i, pos, sym, ty;
  long long rip_addr, target_addr, disp;
  long long gl_idx, j, globals_end, floats_end;
  globals_end = (elf_data_vaddr + (gl_count * 8));
  floats_end = (globals_end + (arr_len(elf_flt_data) * 8));
  i = 0;
  while ((i < arr_len(x64_enc_rel_pos))) {
    pos = arr_get(x64_enc_rel_pos, i);
    sym = arr_get(x64_enc_rel_sym, i);
    ty = arr_get(x64_enc_rel_ty, i);
    rip_addr = (((ELF_BASE + elf_code_off) + pos) + 4);
    if ((ty == 0)) {
      gl_idx = (-1);
      j = 0;
      while ((j < gl_count)) {
        if (elf_packed_eq((*(long long*)((uintptr_t)((gl_names + (j * 8))))), sym)) {
          gl_idx = j;
        }
        j = (j + 1);
      }
      if ((gl_idx >= 0)) {
        target_addr = (elf_data_vaddr + (gl_idx * 8));
      } else {
        target_addr = elf_data_vaddr;
      }
    }
    if ((ty == 1)) {
      target_addr = (floats_end + arr_get(elf_sdata_off, sym));
    }
    if ((ty == 2)) {
      target_addr = (globals_end + (sym * 8));
    }
    disp = (target_addr - rip_addr);
    x64_enc_patch32(pos, disp);
    i = (i + 1);
  }
  return 0;
  return 0;
}

uint8_t elf_packed_eq(long long a, long long b) {
  long long sa, la, sb, lb, i;
  if ((a == b)) {
    return 1;
  }
  if ((a < 0)) {
    return 0;
  }
  if ((b < 0)) {
    return 0;
  }
  sa = unpack_s(a);
  la = unpack_l(a);
  sb = unpack_s(b);
  lb = unpack_l(b);
  if ((la != lb)) {
    return 0;
  }
  i = 0;
  while ((i < la)) {
    if ((((long long)(*(uint8_t*)((uintptr_t)(((vbuf + sa) + i))))) != ((long long)(*(uint8_t*)((uintptr_t)(((vbuf + sb) + i))))))) {
      return 0;
    }
    i = (i + 1);
  }
  return 1;
  return 0;
}

uint8_t write_elf(long long filename, long long main_label, long long gl_names, long long gl_count) {
  long long fd, written, code_size, main_code_off;
  long long i, flt_bits;
  code_size = x64_enc_pos;
  main_code_off = arr_get(x64_enc_lbl_off, main_label);
  elf_compute_layout(code_size, gl_count);
  elf_entry = ((ELF_BASE + elf_code_off) + main_code_off);
  elf_resolve_relocations(gl_names, gl_count);
  elf_pos = 0;
  elf_write_header();
  elf_write_phdr(1, 5, 0, ELF_BASE, elf_text_filesz, elf_text_memsz, ELF_PAGE);
  if (elf_has_data) {
    elf_write_phdr(1, 6, elf_data_off, elf_data_vaddr, elf_data_filesz, elf_data_memsz, ELF_PAGE);
  }
  i = 0;
  while ((i < code_size)) {
    (*(uint8_t*)((uintptr_t)((elf_buf + elf_pos))) = (uint8_t)(((long long)(*(uint8_t*)((uintptr_t)((x64_enc_buf + i)))))));
    elf_pos = (elf_pos + 1);
    i = (i + 1);
  }
  if (elf_has_data) {
    elf_pad((elf_data_off - elf_pos));
    elf_pad((gl_count * 8));
    i = 0;
    while ((i < arr_len(elf_flt_data))) {
      flt_bits = arr_get(elf_flt_data, i);
      elf_w32((flt_bits & 0xFFFFFFFF));
      elf_w32(((flt_bits >> 32) & 0xFFFFFFFF));
      i = (i + 1);
    }
    i = 0;
    while ((i < elf_sdata_size)) {
      (*(uint8_t*)((uintptr_t)((elf_buf + elf_pos))) = (uint8_t)(((long long)(*(uint8_t*)((uintptr_t)((elf_sdata + i)))))));
      elf_pos = (elf_pos + 1);
      i = (i + 1);
    }
  }
  fd = bpp_sys_open((const char*)((uintptr_t)(filename)), 0x601);
  written = bpp_sys_write(fd, (void*)((uintptr_t)(elf_buf)), elf_pos);
  bpp_sys_close(fd);
  return 0;
  return 0;
}

uint8_t init_bo(void) {
  bo_buf = (long long)malloc(262144);
  bo_pos = 0;
  bo_size = 0;
  return 0;
  return 0;
}

uint8_t bo_reset(void) {
  bo_pos = 0;
  bo_size = 0;
  return 0;
  return 0;
}

uint8_t bo_write_u8(long long val) {
  (*(uint8_t*)((uintptr_t)((bo_buf + bo_pos))) = (uint8_t)((val & 0xFF)));
  bo_pos = (bo_pos + 1);
  return 0;
  return 0;
}

uint8_t bo_write_u16(long long val) {
  (*(uint8_t*)((uintptr_t)((bo_buf + bo_pos))) = (uint8_t)((val & 0xFF)));
  (*(uint8_t*)((uintptr_t)(((bo_buf + bo_pos) + 1))) = (uint8_t)(((val >> 8) & 0xFF)));
  bo_pos = (bo_pos + 2);
  return 0;
  return 0;
}

uint8_t bo_write_u32(long long val) {
  (*(uint8_t*)((uintptr_t)((bo_buf + bo_pos))) = (uint8_t)((val & 0xFF)));
  (*(uint8_t*)((uintptr_t)(((bo_buf + bo_pos) + 1))) = (uint8_t)(((val >> 8) & 0xFF)));
  (*(uint8_t*)((uintptr_t)(((bo_buf + bo_pos) + 2))) = (uint8_t)(((val >> 16) & 0xFF)));
  (*(uint8_t*)((uintptr_t)(((bo_buf + bo_pos) + 3))) = (uint8_t)(((val >> 24) & 0xFF)));
  bo_pos = (bo_pos + 4);
  return 0;
  return 0;
}

uint8_t bo_write_u64(long long val) {
  bo_write_u32((val & 0xFFFFFFFF));
  bo_write_u32(((val >> 32) & 0xFFFFFFFF));
  return 0;
  return 0;
}

long long bo_read_u8(void) {
  long long val;
  val = (((long long)(*(uint8_t*)((uintptr_t)((bo_buf + bo_pos))))) & 0xFF);
  bo_pos = (bo_pos + 1);
  return val;
  return 0;
}

long long bo_read_u16(void) {
  long long lo, hi;
  lo = (((long long)(*(uint8_t*)((uintptr_t)((bo_buf + bo_pos))))) & 0xFF);
  hi = (((long long)(*(uint8_t*)((uintptr_t)(((bo_buf + bo_pos) + 1))))) & 0xFF);
  bo_pos = (bo_pos + 2);
  return (lo | (hi << 8));
  return 0;
}

long long bo_read_u32(void) {
  long long b0, b1, b2, b3;
  b0 = (((long long)(*(uint8_t*)((uintptr_t)((bo_buf + bo_pos))))) & 0xFF);
  b1 = (((long long)(*(uint8_t*)((uintptr_t)(((bo_buf + bo_pos) + 1))))) & 0xFF);
  b2 = (((long long)(*(uint8_t*)((uintptr_t)(((bo_buf + bo_pos) + 2))))) & 0xFF);
  b3 = (((long long)(*(uint8_t*)((uintptr_t)(((bo_buf + bo_pos) + 3))))) & 0xFF);
  bo_pos = (bo_pos + 4);
  return (((b0 | (b1 << 8)) | (b2 << 16)) | (b3 << 24));
  return 0;
}

long long bo_read_u64(void) {
  long long lo, hi;
  lo = bo_read_u32();
  hi = bo_read_u32();
  return (lo | (hi << 32));
  return 0;
}

uint8_t bo_write_bytes(long long src, long long sz) {
  long long i;
  i = 0;
  while ((i < sz)) {
    (*(uint8_t*)((uintptr_t)((bo_buf + bo_pos))) = (uint8_t)(((long long)(*(uint8_t*)((uintptr_t)((src + i)))))));
    bo_pos = (bo_pos + 1);
    i = (i + 1);
  }
  return 0;
  return 0;
}

uint8_t bo_read_bytes(long long dst, long long sz) {
  long long i;
  i = 0;
  while ((i < sz)) {
    (*(uint8_t*)((uintptr_t)((dst + i))) = (uint8_t)(((long long)(*(uint8_t*)((uintptr_t)((bo_buf + bo_pos)))))));
    bo_pos = (bo_pos + 1);
    i = (i + 1);
  }
  return 0;
  return 0;
}

uint8_t bo_write_str(long long pv) {
  long long s, l, i;
  s = unpack_s(pv);
  l = unpack_l(pv);
  bo_write_u16(l);
  i = 0;
  while ((i < l)) {
    (*(uint8_t*)((uintptr_t)((bo_buf + bo_pos))) = (uint8_t)(((long long)(*(uint8_t*)((uintptr_t)(((vbuf + s) + i)))))));
    bo_pos = (bo_pos + 1);
    i = (i + 1);
  }
  return 0;
  return 0;
}

long long bo_read_str(void) {
  long long l, start, i;
  l = bo_read_u16();
  start = vbuf_pos;
  i = 0;
  while ((i < l)) {
    (*(uint8_t*)((uintptr_t)((vbuf + vbuf_pos))) = (uint8_t)(((long long)(*(uint8_t*)((uintptr_t)((bo_buf + bo_pos)))))));
    vbuf_pos = (vbuf_pos + 1);
    bo_pos = (bo_pos + 1);
    i = (i + 1);
  }
  return pack(start, l);
  return 0;
}

uint8_t bo_save_file(long long fpath) {
  long long fd;
  fd = bpp_sys_open((const char*)((uintptr_t)(fpath)), 0x601);
  if ((fd < 0)) {
    return (0 - 1);
  }
  bpp_sys_write(fd, (void*)((uintptr_t)(bo_buf)), bo_pos);
  bpp_sys_close(fd);
  return 0;
  return 0;
}

long long bo_load_file(long long fpath) {
  long long fd, nread;
  fd = bpp_sys_open((const char*)((uintptr_t)(fpath)), 0);
  if ((fd < 0)) {
    return (0 - 1);
  }
  nread = bpp_sys_read(fd, (void*)((uintptr_t)(bo_buf)), 262144);
  bpp_sys_close(fd);
  if ((nread <= 0)) {
    return (0 - 1);
  }
  bo_pos = 0;
  bo_size = nread;
  return nread;
  return 0;
}

uint8_t bo_make_path(long long mi) {
  long long fp, fs, fl, i, ch, p;
  if ((bo_pathbuf == 0)) {
    bo_pathbuf = (long long)malloc(512);
  }
  p = 0;
  (*(uint8_t*)((uintptr_t)((bo_pathbuf + p))) = (uint8_t)(46));
  p = (p + 1);
  (*(uint8_t*)((uintptr_t)((bo_pathbuf + p))) = (uint8_t)(98));
  p = (p + 1);
  (*(uint8_t*)((uintptr_t)((bo_pathbuf + p))) = (uint8_t)(112));
  p = (p + 1);
  (*(uint8_t*)((uintptr_t)((bo_pathbuf + p))) = (uint8_t)(112));
  p = (p + 1);
  (*(uint8_t*)((uintptr_t)((bo_pathbuf + p))) = (uint8_t)(95));
  p = (p + 1);
  (*(uint8_t*)((uintptr_t)((bo_pathbuf + p))) = (uint8_t)(99));
  p = (p + 1);
  (*(uint8_t*)((uintptr_t)((bo_pathbuf + p))) = (uint8_t)(97));
  p = (p + 1);
  (*(uint8_t*)((uintptr_t)((bo_pathbuf + p))) = (uint8_t)(99));
  p = (p + 1);
  (*(uint8_t*)((uintptr_t)((bo_pathbuf + p))) = (uint8_t)(104));
  p = (p + 1);
  (*(uint8_t*)((uintptr_t)((bo_pathbuf + p))) = (uint8_t)(101));
  p = (p + 1);
  (*(uint8_t*)((uintptr_t)((bo_pathbuf + p))) = (uint8_t)(47));
  p = (p + 1);
  fp = arr_get(diag_file_names, mi);
  fs = unpack_s(fp);
  fl = unpack_l(fp);
  i = 0;
  while ((i < fl)) {
    ch = ((long long)(*(uint8_t*)((uintptr_t)(((fname_buf + fs) + i)))));
    if ((ch == 47)) {
      ch = 95;
    }
    (*(uint8_t*)((uintptr_t)((bo_pathbuf + p))) = (uint8_t)(ch));
    p = (p + 1);
    i = (i + 1);
  }
  i = (p - 1);
  while ((i > 11)) {
    if ((((long long)(*(uint8_t*)((uintptr_t)((bo_pathbuf + i))))) == 46)) {
      (*(uint8_t*)((uintptr_t)(((bo_pathbuf + i) + 1))) = (uint8_t)(98));
      (*(uint8_t*)((uintptr_t)(((bo_pathbuf + i) + 2))) = (uint8_t)(111));
      (*(uint8_t*)((uintptr_t)(((bo_pathbuf + i) + 3))) = (uint8_t)(0));
      return (i + 3);
    }
    i = (i - 1);
  }
  (*(uint8_t*)((uintptr_t)((bo_pathbuf + p))) = (uint8_t)(46));
  p = (p + 1);
  (*(uint8_t*)((uintptr_t)((bo_pathbuf + p))) = (uint8_t)(98));
  p = (p + 1);
  (*(uint8_t*)((uintptr_t)((bo_pathbuf + p))) = (uint8_t)(111));
  p = (p + 1);
  (*(uint8_t*)((uintptr_t)((bo_pathbuf + p))) = (uint8_t)(0));
  return p;
  return 0;
}

uint8_t bo_write_code_arm64(long long code_s, long long fn_s, long long str_s, long long flt_s, long long rel_s) {
  long long code_sz, fc, rc, sc, xc, i, lbl, off;
  code_sz = (enc_pos - code_s);
  bo_write_u32(code_sz);
  bo_write_bytes((enc_buf + code_s), code_sz);
  fc = (arr_len(a64_fn_name) - fn_s);
  bo_write_u16(fc);
  i = fn_s;
  while ((i < arr_len(a64_fn_name))) {
    bo_write_str(arr_get(a64_fn_name, i));
    lbl = arr_get(a64_fn_lbl, i);
    off = (arr_get(enc_lbl_off, lbl) - code_s);
    bo_write_u32(off);
    i = (i + 1);
  }
  rc = (arr_len(enc_rel_pos) - rel_s);
  bo_write_u16(rc);
  i = rel_s;
  while ((i < arr_len(enc_rel_pos))) {
    bo_write_u32((arr_get(enc_rel_pos, i) - code_s));
    off = arr_get(enc_rel_ty, i);
    bo_write_u8(off);
    if ((off == 1)) {
      bo_write_u32((arr_get(enc_rel_sym, i) - str_s));
    } else {
      if ((off == 2)) {
        bo_write_u32((arr_get(enc_rel_sym, i) - flt_s));
      } else {
        lbl = arr_get(enc_rel_sym, i);
        if ((lbl < 0)) {
          bo_write_u8(1);
          bo_write_u64(lbl);
        } else {
          bo_write_u8(0);
          bo_write_str(lbl);
        }
      }
    }
    i = (i + 1);
  }
  sc = (arr_len(a64_str_tbl) - str_s);
  bo_write_u16(sc);
  i = str_s;
  while ((i < arr_len(a64_str_tbl))) {
    bo_write_str(arr_get(a64_str_tbl, i));
    i = (i + 1);
  }
  xc = (arr_len(a64_flt_tbl) - flt_s);
  bo_write_u16(xc);
  i = flt_s;
  while ((i < arr_len(a64_flt_tbl))) {
    bo_write_str(arr_get(a64_flt_tbl, i));
    i = (i + 1);
  }
  return 0;
  return 0;
}

long long bo_load_code_arm64(void) {
  long long code_sz, fc, rc, sc, xc, i, name_p, off, lbl;
  long long base, base_str, base_flt, ty, sym;
  base = enc_pos;
  code_sz = bo_read_u32();
  bo_read_bytes((enc_buf + base), code_sz);
  enc_pos = (base + code_sz);
  fc = bo_read_u16();
  i = 0;
  while ((i < fc)) {
    name_p = bo_read_str();
    off = bo_read_u32();
    a64_fn_name = arr_push(a64_fn_name, name_p);
    lbl = enc_new_label();
    enc_def_label_at(lbl, (base + off));
    a64_fn_lbl = arr_push(a64_fn_lbl, lbl);
    a64_fn_par = arr_push(a64_fn_par, 0);
    a64_fn_pcnt = arr_push(a64_fn_pcnt, 0);
    a64_fn_body = arr_push(a64_fn_body, 0);
    a64_fn_bcnt = arr_push(a64_fn_bcnt, 0);
    a64_fn_fidx = arr_push(a64_fn_fidx, 0);
    i = (i + 1);
  }
  base_str = arr_len(a64_str_tbl);
  base_flt = arr_len(a64_flt_tbl);
  rc = bo_read_u16();
  i = 0;
  while ((i < rc)) {
    off = bo_read_u32();
    ty = bo_read_u8();
    if ((ty == 1)) {
      sym = (bo_read_u32() + base_str);
    } else {
      if ((ty == 2)) {
        sym = (bo_read_u32() + base_flt);
      } else {
        if ((bo_read_u8() == 1)) {
          sym = bo_read_u64();
        } else {
          sym = bo_read_str();
        }
      }
    }
    enc_add_reloc((base + off), sym, ty);
    i = (i + 1);
  }
  sc = bo_read_u16();
  i = 0;
  while ((i < sc)) {
    a64_str_tbl = arr_push(a64_str_tbl, bo_read_str());
    i = (i + 1);
  }
  xc = bo_read_u16();
  i = 0;
  while ((i < xc)) {
    a64_flt_tbl = arr_push(a64_flt_tbl, bo_read_str());
    i = (i + 1);
  }
  return enc_pos;
  return 0;
}

uint8_t bo_write_code_x64(long long code_s, long long fn_s, long long str_s, long long flt_s, long long rel_s) {
  long long code_sz, fc, rc, sc, xc, i, lbl, off;
  code_sz = (x64_enc_pos - code_s);
  bo_write_u32(code_sz);
  bo_write_bytes((x64_enc_buf + code_s), code_sz);
  fc = (arr_len(x64_fn_name) - fn_s);
  bo_write_u16(fc);
  i = fn_s;
  while ((i < arr_len(x64_fn_name))) {
    bo_write_str(arr_get(x64_fn_name, i));
    lbl = arr_get(x64_fn_lbl, i);
    off = (arr_get(x64_enc_lbl_off, lbl) - code_s);
    bo_write_u32(off);
    i = (i + 1);
  }
  rc = (arr_len(x64_enc_rel_pos) - rel_s);
  bo_write_u16(rc);
  i = rel_s;
  while ((i < arr_len(x64_enc_rel_pos))) {
    bo_write_u32((arr_get(x64_enc_rel_pos, i) - code_s));
    off = arr_get(x64_enc_rel_ty, i);
    bo_write_u8(off);
    if ((off == 1)) {
      bo_write_u32((arr_get(x64_enc_rel_sym, i) - str_s));
    } else {
      if ((off == 2)) {
        bo_write_u32((arr_get(x64_enc_rel_sym, i) - flt_s));
      } else {
        lbl = arr_get(x64_enc_rel_sym, i);
        if ((lbl < 0)) {
          bo_write_u8(1);
          bo_write_u64(lbl);
        } else {
          bo_write_u8(0);
          bo_write_str(lbl);
        }
      }
    }
    i = (i + 1);
  }
  sc = (arr_len(x64_str_tbl) - str_s);
  bo_write_u16(sc);
  i = str_s;
  while ((i < arr_len(x64_str_tbl))) {
    bo_write_str(arr_get(x64_str_tbl, i));
    i = (i + 1);
  }
  xc = (arr_len(x64_flt_tbl) - flt_s);
  bo_write_u16(xc);
  i = flt_s;
  while ((i < arr_len(x64_flt_tbl))) {
    bo_write_str(arr_get(x64_flt_tbl, i));
    i = (i + 1);
  }
  return 0;
  return 0;
}

long long bo_load_code_x64(void) {
  long long code_sz, fc, rc, sc, xc, i, name_p, off, lbl;
  long long base, base_str, base_flt, ty, sym;
  base = x64_enc_pos;
  code_sz = bo_read_u32();
  bo_read_bytes((x64_enc_buf + base), code_sz);
  x64_enc_pos = (base + code_sz);
  fc = bo_read_u16();
  i = 0;
  while ((i < fc)) {
    name_p = bo_read_str();
    off = bo_read_u32();
    x64_fn_name = arr_push(x64_fn_name, name_p);
    lbl = x64_enc_new_label();
    x64_enc_def_label_at(lbl, (base + off));
    x64_fn_lbl = arr_push(x64_fn_lbl, lbl);
    x64_fn_par = arr_push(x64_fn_par, 0);
    x64_fn_pcnt = arr_push(x64_fn_pcnt, 0);
    x64_fn_body = arr_push(x64_fn_body, 0);
    x64_fn_bcnt = arr_push(x64_fn_bcnt, 0);
    x64_fn_fidx = arr_push(x64_fn_fidx, 0);
    i = (i + 1);
  }
  base_str = arr_len(x64_str_tbl);
  base_flt = arr_len(x64_flt_tbl);
  rc = bo_read_u16();
  i = 0;
  while ((i < rc)) {
    off = bo_read_u32();
    ty = bo_read_u8();
    if ((ty == 1)) {
      sym = (bo_read_u32() + base_str);
    } else {
      if ((ty == 2)) {
        sym = (bo_read_u32() + base_flt);
      } else {
        if ((bo_read_u8() == 1)) {
          sym = bo_read_u64();
        } else {
          sym = bo_read_str();
        }
      }
    }
    x64_enc_add_reloc((base + off), sym, ty);
    i = (i + 1);
  }
  sc = bo_read_u16();
  i = 0;
  while ((i < sc)) {
    x64_str_tbl = arr_push(x64_str_tbl, bo_read_str());
    i = (i + 1);
  }
  xc = bo_read_u16();
  i = 0;
  while ((i < xc)) {
    x64_flt_tbl = arr_push(x64_flt_tbl, bo_read_str());
    i = (i + 1);
  }
  return x64_enc_pos;
  return 0;
}

uint8_t bo_resolve_calls_arm64(void) {
  long long i, nc, pos, sym, fi, target, off, ins;
  i = 0;
  while ((i < arr_len(enc_rel_pos))) {
    if ((arr_get(enc_rel_ty, i) == 4)) {
      pos = arr_get(enc_rel_pos, i);
      sym = arr_get(enc_rel_sym, i);
      fi = (arr_len(a64_fn_name) - 1);
      target = (0 - 1);
      while ((fi >= 0)) {
        if (a64_str_eq_packed(sym, arr_get(a64_fn_name, fi))) {
          target = arr_get(enc_lbl_off, arr_get(a64_fn_lbl, fi));
          fi = (0 - 1);
        }
        fi = (fi - 1);
      }
      if ((target >= 0)) {
        off = ((target - pos) / 4);
        ins = (0x94000000 | (off & 0x3FFFFFF));
        enc_patch32(pos, ins);
      }
    }
    i = (i + 1);
  }
  nc = 0;
  i = 0;
  while ((i < arr_len(enc_rel_pos))) {
    if ((arr_get(enc_rel_ty, i) != 4)) {
      arr_set(enc_rel_pos, nc, arr_get(enc_rel_pos, i));
      arr_set(enc_rel_sym, nc, arr_get(enc_rel_sym, i));
      arr_set(enc_rel_ty, nc, arr_get(enc_rel_ty, i));
      nc = (nc + 1);
    }
    i = (i + 1);
  }
  while ((arr_len(enc_rel_pos) > nc)) {
    arr_pop(enc_rel_pos);
  }
  while ((arr_len(enc_rel_sym) > nc)) {
    arr_pop(enc_rel_sym);
  }
  while ((arr_len(enc_rel_ty) > nc)) {
    arr_pop(enc_rel_ty);
  }
  return 0;
  return 0;
}

uint8_t bo_resolve_calls_x64(void) {
  long long i, nc, pos, sym, fi, target, off;
  i = 0;
  while ((i < arr_len(x64_enc_rel_pos))) {
    if ((arr_get(x64_enc_rel_ty, i) == 4)) {
      pos = arr_get(x64_enc_rel_pos, i);
      sym = arr_get(x64_enc_rel_sym, i);
      fi = (arr_len(x64_fn_name) - 1);
      target = (0 - 1);
      while ((fi >= 0)) {
        if (x64_str_eq_packed(sym, arr_get(x64_fn_name, fi))) {
          target = arr_get(x64_enc_lbl_off, arr_get(x64_fn_lbl, fi));
          fi = (0 - 1);
        }
        fi = (fi - 1);
      }
      if ((target >= 0)) {
        off = (target - (pos + 4));
        x64_enc_patch32(pos, off);
      }
    }
    i = (i + 1);
  }
  nc = 0;
  i = 0;
  while ((i < arr_len(x64_enc_rel_pos))) {
    if ((arr_get(x64_enc_rel_ty, i) != 4)) {
      arr_set(x64_enc_rel_pos, nc, arr_get(x64_enc_rel_pos, i));
      arr_set(x64_enc_rel_sym, nc, arr_get(x64_enc_rel_sym, i));
      arr_set(x64_enc_rel_ty, nc, arr_get(x64_enc_rel_ty, i));
      nc = (nc + 1);
    }
    i = (i + 1);
  }
  while ((arr_len(x64_enc_rel_pos) > nc)) {
    arr_pop(x64_enc_rel_pos);
  }
  while ((arr_len(x64_enc_rel_sym) > nc)) {
    arr_pop(x64_enc_rel_sym);
  }
  while ((arr_len(x64_enc_rel_ty) > nc)) {
    arr_pop(x64_enc_rel_ty);
  }
  return 0;
  return 0;
}

uint8_t bo_write_header(long long mi, long long target) {
  long long k, dc;
  bo_write_u8(66);
  bo_write_u8(79);
  bo_write_u8(2);
  bo_write_u8(target);
  bo_write_u64(cache_manifest_hash());
  bo_write_u64(arr_get(mod_hashes, mi));
  dc = 0;
  k = 0;
  while ((k < arr_len(dep_from))) {
    if ((arr_get(dep_from, k) == mi)) {
      dc = (dc + 1);
    }
    k = (k + 1);
  }
  bo_write_u16(dc);
  k = 0;
  while ((k < arr_len(dep_from))) {
    if ((arr_get(dep_from, k) == mi)) {
      bo_write_u64(arr_get(mod_hashes, arr_get(dep_to, k)));
    }
    k = (k + 1);
  }
  return 0;
  return 0;
}

uint8_t bo_check_header(long long mi) {
  long long m0, m1, ver, tgt, mh, sh, dc, k, dh, dep_mi, j, match;
  m0 = bo_read_u8();
  m1 = bo_read_u8();
  if ((m0 != 66)) {
    return 0;
  }
  if ((m1 != 79)) {
    return 0;
  }
  ver = bo_read_u8();
  if ((ver != 2)) {
    return 0;
  }
  tgt = bo_read_u8();
  mh = bo_read_u64();
  if ((mh != cache_manifest_hash())) {
    return 0;
  }
  sh = bo_read_u64();
  if ((sh != arr_get(mod_hashes, mi))) {
    return 0;
  }
  dc = bo_read_u16();
  k = 0;
  j = 0;
  while ((k < dc)) {
    dh = bo_read_u64();
    match = 0;
    while ((j < arr_len(dep_from))) {
      if ((arr_get(dep_from, j) == mi)) {
        dep_mi = arr_get(dep_to, j);
        j = (j + 1);
        if ((dh != arr_get(mod_hashes, dep_mi))) {
          return 0;
        }
        match = 1;
        k = (k + 1);
        if ((k >= dc)) {
          return 1;
        }
      } else {
        j = (j + 1);
      }
    }
    if ((match == 0)) {
      return 0;
    }
  }
  return 1;
  return 0;
}

uint8_t bo_write_export(long long mi) {
  long long i, fc, rec, name_p, pcnt, j, hints, h;
  long long fields, flds;
  fc = 0;
  i = 0;
  while ((i < func_cnt)) {
    if ((arr_get(func_mods, i) == mi)) {
      fc = (fc + 1);
    }
    i = (i + 1);
  }
  bo_write_u16(fc);
  i = 0;
  while ((i < func_cnt)) {
    if ((arr_get(func_mods, i) == mi)) {
      rec = (*(long long*)((uintptr_t)((funcs + (i * 8)))));
      name_p = (*(long long*)((uintptr_t)((rec + FN_NAME))));
      pcnt = (*(long long*)((uintptr_t)((rec + FN_PCNT))));
      hints = (*(long long*)((uintptr_t)((rec + FN_HINTS))));
      bo_write_str(name_p);
      bo_write_u8(get_fn_ret_type(i));
      bo_write_u8(pcnt);
      j = 0;
      while ((j < pcnt)) {
        bo_write_u8(get_fn_par_type(i, j));
        if ((hints != 0)) {
          h = (*(long long*)((uintptr_t)((hints + (j * 8)))));
        } else {
          h = 0;
        }
        bo_write_u8(h);
        j = (j + 1);
      }
    }
    i = (i + 1);
  }
  fc = 0;
  i = 0;
  while ((i < arr_len(sd_names))) {
    if ((arr_get(struct_mods, i) == mi)) {
      fc = (fc + 1);
    }
    i = (i + 1);
  }
  bo_write_u16(fc);
  i = 0;
  while ((i < arr_len(sd_names))) {
    if ((arr_get(struct_mods, i) == mi)) {
      bo_write_str(arr_get(sd_names, i));
      flds = arr_get(sd_fcounts, i);
      bo_write_u16(flds);
      fields = arr_get(sd_fields, i);
      j = 0;
      while ((j < flds)) {
        bo_write_str((*(long long*)((uintptr_t)((fields + (j * 8))))));
        j = (j + 1);
      }
    }
    i = (i + 1);
  }
  fc = 0;
  i = 0;
  while ((i < arr_len(ev_names))) {
    if ((arr_get(enum_mods, i) == mi)) {
      fc = (fc + 1);
    }
    i = (i + 1);
  }
  bo_write_u16(fc);
  i = 0;
  while ((i < arr_len(ev_names))) {
    if ((arr_get(enum_mods, i) == mi)) {
      bo_write_str(arr_get(ev_names, i));
      bo_write_u64(arr_get(ev_values, i));
    }
    i = (i + 1);
  }
  fc = 0;
  i = 0;
  while ((i < glob_cnt)) {
    if ((arr_get(glob_mods, i) == mi)) {
      fc = (fc + 1);
    }
    i = (i + 1);
  }
  bo_write_u16(fc);
  i = 0;
  while ((i < glob_cnt)) {
    if ((arr_get(glob_mods, i) == mi)) {
      bo_write_str(arr_get(globals, i));
    }
    i = (i + 1);
  }
  fc = 0;
  i = 0;
  while ((i < ext_cnt)) {
    if ((arr_get(extern_mods, i) == mi)) {
      fc = (fc + 1);
    }
    i = (i + 1);
  }
  bo_write_u16(fc);
  i = 0;
  while ((i < ext_cnt)) {
    if ((arr_get(extern_mods, i) == mi)) {
      rec = (*(long long*)((uintptr_t)((externs + (i * 8)))));
      bo_write_str((*(long long*)((uintptr_t)((rec + EX_NAME)))));
      bo_write_str((*(long long*)((uintptr_t)((rec + EX_LIB)))));
      bo_write_str((*(long long*)((uintptr_t)((rec + EX_RET)))));
      pcnt = (*(long long*)((uintptr_t)((rec + EX_ACNT))));
      bo_write_u8(pcnt);
      j = 0;
      while ((j < pcnt)) {
        bo_write_str((*(long long*)((uintptr_t)(((*(long long*)((uintptr_t)((rec + 32)))) + (j * 8))))));
        j = (j + 1);
      }
    }
    i = (i + 1);
  }
  return 0;
  return 0;
}

uint8_t bo_read_export(long long mi) {
  long long fc, i, j, name_p, rty, pcnt, rec, hints, h;
  long long flds, fields, idx, ac, args;
  fc = bo_read_u16();
  i = 0;
  while ((i < fc)) {
    name_p = bo_read_str();
    rty = bo_read_u8();
    pcnt = bo_read_u8();
    set_func_type(name_p, rty);
    hints = 0;
    if ((pcnt > 0)) {
      hints = (long long)malloc((pcnt * 8));
    }
    j = 0;
    while ((j < pcnt)) {
      bo_read_u8();
      h = bo_read_u8();
      if ((hints != 0)) {
        *(long long*)((uintptr_t)((hints + (j * 8)))) = h;
      }
      j = (j + 1);
    }
    rec = (long long)malloc(NODE_SZ);
    *(long long*)((uintptr_t)((rec + FN_TYPE))) = 22;
    *(long long*)((uintptr_t)((rec + FN_NAME))) = name_p;
    *(long long*)((uintptr_t)((rec + FN_PARS))) = 0;
    *(long long*)((uintptr_t)((rec + FN_PCNT))) = pcnt;
    *(long long*)((uintptr_t)((rec + FN_BODY))) = 0;
    *(long long*)((uintptr_t)((rec + FN_BCNT))) = 0;
    *(long long*)((uintptr_t)((rec + FN_HINTS))) = hints;
    funcs = arr_push(funcs, rec);
    func_cnt = arr_len(funcs);
    func_mods = arr_push(func_mods, mi);
    i = (i + 1);
  }
  fc = bo_read_u16();
  i = 0;
  while ((i < fc)) {
    name_p = bo_read_str();
    flds = bo_read_u16();
    sd_names = arr_push(sd_names, name_p);
    sd_fields = arr_push(sd_fields, (long long)malloc((64 * 8)));
    sd_fcounts = arr_push(sd_fcounts, 0);
    sd_sizes = arr_push(sd_sizes, 0);
    struct_mods = arr_push(struct_mods, mi);
    idx = (arr_len(sd_names) - 1);
    j = 0;
    while ((j < flds)) {
      add_struct_field(idx, bo_read_str());
      j = (j + 1);
    }
    i = (i + 1);
  }
  fc = bo_read_u16();
  i = 0;
  while ((i < fc)) {
    name_p = bo_read_str();
    h = bo_read_u64();
    ev_names = arr_push(ev_names, name_p);
    ev_values = arr_push(ev_values, h);
    enum_mods = arr_push(enum_mods, mi);
    i = (i + 1);
  }
  fc = bo_read_u16();
  i = 0;
  while ((i < fc)) {
    name_p = bo_read_str();
    globals = arr_push(globals, name_p);
    glob_cnt = arr_len(globals);
    glob_mods = arr_push(glob_mods, mi);
    i = (i + 1);
  }
  fc = bo_read_u16();
  i = 0;
  while ((i < fc)) {
    rec = (long long)malloc(NODE_SZ);
    *(long long*)((uintptr_t)((rec + EX_TYPE))) = 21;
    *(long long*)((uintptr_t)((rec + EX_NAME))) = bo_read_str();
    *(long long*)((uintptr_t)((rec + EX_LIB))) = bo_read_str();
    *(long long*)((uintptr_t)((rec + EX_RET))) = bo_read_str();
    ac = bo_read_u8();
    args = (long long)malloc(((ac * 8) + 8));
    j = 0;
    while ((j < ac)) {
      *(long long*)((uintptr_t)((args + (j * 8)))) = bo_read_str();
      j = (j + 1);
    }
    *(long long*)((uintptr_t)((rec + EX_ARGS))) = args;
    *(long long*)((uintptr_t)((rec + EX_ACNT))) = ac;
    externs = arr_push(externs, rec);
    ext_cnt = arr_len(externs);
    extern_mods = arr_push(extern_mods, mi);
    i = (i + 1);
  }
  return 0;
  return 0;
}

uint8_t main_bpp(void) {
  long long arg_len, arg_ptr, mode, c0, c1, out_name, argi, nargs;
  long long main_fi, main_lbl, start_lbl, i, fp, fs, fl;
  long long flag_permod, flag_mono, mi, mk, code_start;
  long long fn_s, str_s, flt_s, rel_s;
  mode = 2;
  out_name = 0;
  flag_permod = 0;
  flag_mono = 0;
  init_array();
  init_defs();
  init_internal();
  init_diag();
  init_import();
  init_parser();
  init_lexer();
  init_types();
  init_dispatch();
  init_emitter();
  init_enc_arm64();
  init_codegen_arm64();
  init_macho();
  init_x64_enc();
  init_codegen_x86_64();
  init_elf();
  init_bo();
  long long a;
  nargs = ((long long)_bpp_argc);
  argi = 1;
  arg_ptr = 0;
  while ((argi < nargs)) {
    a = ((long long)_bpp_argv[(int)(argi)]);
    if ((a == 0)) {
      argi = (argi + 1);
    }
    if ((a == 0)) {
      return 1;
    }
    c0 = (long long)((uint8_t*)((uintptr_t)(a)))[(0)];
    c1 = (long long)((uint8_t*)((uintptr_t)(a)))[(1)];
    if ((c0 == 45)) {
      if ((c1 == 45)) {
        if (((long long)((uint8_t*)((uintptr_t)(a)))[(2)] == 97)) {
          if (((long long)((uint8_t*)((uintptr_t)(a)))[(3)] == 115)) {
            if (((long long)((uint8_t*)((uintptr_t)(a)))[(4)] == 109)) {
              mode = 1;
            }
          }
        }
        if (((long long)((uint8_t*)((uintptr_t)(a)))[(2)] == 98)) {
          if (((long long)((uint8_t*)((uintptr_t)(a)))[(3)] == 105)) {
            if (((long long)((uint8_t*)((uintptr_t)(a)))[(4)] == 110)) {
              mode = 2;
            }
          }
        }
        if (((long long)((uint8_t*)((uintptr_t)(a)))[(2)] == 99)) {
          mode = 0;
        }
        if (((long long)((uint8_t*)((uintptr_t)(a)))[(2)] == 108)) {
          if (((long long)((uint8_t*)((uintptr_t)(a)))[(3)] == 105)) {
            if (((long long)((uint8_t*)((uintptr_t)(a)))[(4)] == 110)) {
              mode = 3;
            }
          }
        }
        if (((long long)((uint8_t*)((uintptr_t)(a)))[(2)] == 115)) {
          if (((long long)((uint8_t*)((uintptr_t)(a)))[(3)] == 104)) {
            if (((long long)((uint8_t*)((uintptr_t)(a)))[(4)] == 111)) {
              mode = 4;
            }
          }
        }
        if (((long long)((uint8_t*)((uintptr_t)(a)))[(2)] == 112)) {
          if (((long long)((uint8_t*)((uintptr_t)(a)))[(3)] == 101)) {
            if (((long long)((uint8_t*)((uintptr_t)(a)))[(4)] == 114)) {
              flag_permod = 1;
            }
          }
        }
        if (((long long)((uint8_t*)((uintptr_t)(a)))[(2)] == 109)) {
          if (((long long)((uint8_t*)((uintptr_t)(a)))[(3)] == 111)) {
            if (((long long)((uint8_t*)((uintptr_t)(a)))[(4)] == 110)) {
              flag_mono = 1;
            }
          }
        }
        if (((long long)((uint8_t*)((uintptr_t)(a)))[(2)] == 105)) {
          if (((long long)((uint8_t*)((uintptr_t)(a)))[(3)] == 110)) {
            if (((long long)((uint8_t*)((uintptr_t)(a)))[(4)] == 99)) {
              flag_incr = 1;
            }
          }
        }
      }
      if ((c1 == 111)) {
        argi = (argi + 1);
        out_name = ((long long)_bpp_argv[(int)(argi)]);
      }
    } else {
      arg_ptr = a;
    }
    argi = (argi + 1);
  }
  if ((mode == 3)) {
    (*(uint8_t*)((uintptr_t)((_imp_target + 0))) = (uint8_t)(108));
    (*(uint8_t*)((uintptr_t)((_imp_target + 1))) = (uint8_t)(105));
    (*(uint8_t*)((uintptr_t)((_imp_target + 2))) = (uint8_t)(110));
    (*(uint8_t*)((uintptr_t)((_imp_target + 3))) = (uint8_t)(117));
    (*(uint8_t*)((uintptr_t)((_imp_target + 4))) = (uint8_t)(120));
    _imp_target_len = 5;
  }
  arg_len = copy_cstr(pathbuf, arg_ptr);
  process_file(pathbuf, arg_len);
  topo_sort();
  cache_load();
  propagate_stale();
  if ((mode == 4)) {
    long long mi, mk, fp, fs, fl, mn;
    mn = arr_len(diag_file_starts);
    diag_str((long long)"=== Module Graph (");
    diag_int(mn);
    diag_str((long long)" modules) ===");
    diag_newline();
    mi = 0;
    while ((mi < mn)) {
      if (arr_get(mod_stale, mi)) {
        diag_str((long long)"[STALE] ");
      } else {
        diag_str((long long)"[clean] ");
      }
      fp = arr_get(diag_file_names, mi);
      fs = unpack_s(fp);
      fl = unpack_l(fp);
      diag_buf((fname_buf + fs), fl);
      diag_newline();
      mk = 0;
      while ((mk < arr_len(dep_from))) {
        if ((arr_get(dep_from, mk) == mi)) {
          diag_str((long long)"  -> ");
          fp = arr_get(diag_file_names, arr_get(dep_to, mk));
          fs = unpack_s(fp);
          fl = unpack_l(fp);
          diag_buf((fname_buf + fs), fl);
          diag_newline();
        }
        mk = (mk + 1);
      }
      mi = (mi + 1);
    }
    diag_str((long long)"=== Topological Order ===");
    diag_newline();
    mi = 0;
    while ((mi < arr_len(mod_topo))) {
      mk = arr_get(mod_topo, mi);
      fp = arr_get(diag_file_names, mk);
      fs = unpack_s(fp);
      fl = unpack_l(fp);
      diag_int((mi + 1));
      diag_str((long long)". ");
      diag_buf((fname_buf + fs), fl);
      diag_newline();
      mi = (mi + 1);
    }
    cache_save();
    return 0;
  }
  src = outbuf;
  src_len = outbuf_len;
  long long flag_incr;
  flag_incr = 1;
  if (flag_mono) {
    flag_incr = 0;
  }
  if ((mode == 0)) {
    flag_incr = 0;
  }
  if ((mode == 1)) {
    flag_incr = 0;
  }
  if (flag_incr) {
    mk = arr_len(diag_file_starts);
    long long tok_before;
    if ((mode == 2)) {
      a64_bin_mode = 1;
      a64_mod_init();
    }
    if ((mode == 3)) {
      x64_mod_init();
    }
    i = 0;
    while ((i < mk)) {
      bo_make_path(i);
      if ((arr_get(mod_stale, i) == 0)) {
        if ((bo_load_file(bo_pathbuf) > 0)) {
          if (bo_check_header(i)) {
            bo_read_export(i);
            if ((mode == 2)) {
              bo_load_code_arm64();
            }
            if ((mode == 3)) {
              bo_load_code_x64();
            }
            i = (i + 1);
          } else {
            arr_set(mod_stale, i, 1);
          }
        } else {
          arr_set(mod_stale, i, 1);
        }
      }
      if ((arr_get(mod_stale, i) == 1)) {
        tok_before = tok_cnt;
        lex_module(i);
        parse_module(tok_cnt);
        infer_module(i);
        if ((mode == 2)) {
          fn_s = arr_len(a64_fn_name);
          str_s = arr_len(a64_str_tbl);
          flt_s = arr_len(a64_flt_tbl);
          rel_s = arr_len(enc_rel_pos);
          code_start = emit_module_arm64(i);
          bo_reset();
          bo_write_header(i, 0);
          bo_write_export(i);
          bo_write_code_arm64(code_start, fn_s, str_s, flt_s, rel_s);
          bo_save_file(bo_pathbuf);
        }
        if ((mode == 3)) {
          fn_s = arr_len(x64_fn_name);
          str_s = arr_len(x64_str_tbl);
          flt_s = arr_len(x64_flt_tbl);
          rel_s = arr_len(x64_enc_rel_pos);
          code_start = emit_module_x86_64(i);
          bo_reset();
          bo_write_header(i, 1);
          bo_write_export(i);
          bo_write_code_x64(code_start, fn_s, str_s, flt_s, rel_s);
          bo_save_file(bo_pathbuf);
        }
        i = (i + 1);
      }
    }
    if ((mode == 2)) {
      arr_clear(a64_gl_name);
      mk = 0;
      while ((mk < glob_cnt)) {
        fn_s = (*(long long*)((uintptr_t)((globals + (mk * 8)))));
        rel_s = (-1);
        mi = 0;
        while ((mi < arr_len(a64_gl_name))) {
          if (a64_str_eq_packed(fn_s, arr_get(a64_gl_name, mi))) {
            rel_s = mi;
          }
          mi = (mi + 1);
        }
        if ((rel_s < 0)) {
          a64_gl_name = arr_push(a64_gl_name, fn_s);
        }
        mk = (mk + 1);
      }
      a64_gl_name = arr_push(a64_gl_name, a64_argc_sym);
      a64_gl_name = arr_push(a64_gl_name, a64_argv_sym);
      bo_resolve_calls_arm64();
    }
    if ((mode == 3)) {
      arr_clear(x64_gl_name);
      mk = 0;
      while ((mk < glob_cnt)) {
        fn_s = (*(long long*)((uintptr_t)((globals + (mk * 8)))));
        rel_s = (-1);
        mi = 0;
        while ((mi < arr_len(x64_gl_name))) {
          if (x64_str_eq_packed(fn_s, arr_get(x64_gl_name, mi))) {
            rel_s = mi;
          }
          mi = (mi + 1);
        }
        if ((rel_s < 0)) {
          x64_gl_name = arr_push(x64_gl_name, fn_s);
        }
        mk = (mk + 1);
      }
      x64_gl_name = arr_push(x64_gl_name, x64_argc_sym);
      x64_gl_name = arr_push(x64_gl_name, x64_argv_sym);
      bo_resolve_calls_x64();
      start_lbl = x64_emit_start_stub();
    }
  }
  if ((flag_incr == 0)) {
    lex_all();
    parse_program();
    run_types();
    run_dispatch();
    run_validate();
  }
  if ((mode == 2)) {
    a64_bin_mode = 1;
    if ((flag_incr == 0)) {
      if (flag_permod) {
        a64_mod_init();
        mk = arr_len(diag_file_starts);
        i = 0;
        while ((i < mk)) {
          emit_module_arm64(i);
          i = (i + 1);
        }
        bo_resolve_calls_arm64();
      } else {
        emit_all_arm64();
      }
    }
    main_fi = (-1);
    i = 0;
    while ((i < arr_len(a64_fn_name))) {
      if (a64_str_eq(arr_get(a64_fn_name, i), (long long)"main", 4)) {
        main_fi = i;
      }
      i = (i + 1);
    }
    main_lbl = arr_get(a64_fn_lbl, main_fi);
    i = 0;
    while ((i < arr_len(a64_flt_tbl))) {
      fp = arr_get(a64_flt_tbl, i);
      fs = unpack_s(fp);
      mo_add_float((a64_sbuf + fs));
      i = (i + 1);
    }
    i = 0;
    while ((i < arr_len(a64_str_tbl))) {
      fp = arr_get(a64_str_tbl, i);
      fs = unpack_s(fp);
      fl = unpack_l(fp);
      mo_add_string((a64_sbuf + fs), fl);
      i = (i + 1);
    }
    long long ext_rec, ext_name, ext_lib;
    i = 0;
    while ((i < ext_cnt)) {
      ext_rec = (*(long long*)((uintptr_t)((externs + (i * 8)))));
      ext_name = (*(long long*)((uintptr_t)((ext_rec + EX_NAME))));
      ext_lib = (*(long long*)((uintptr_t)((ext_rec + EX_LIB))));
      mo_add_got(ext_name, ext_lib);
      i = (i + 1);
    }
    if ((out_name != 0)) {
      long long ol;
      ol = copy_cstr(pathbuf, out_name);
      (*(uint8_t*)((uintptr_t)((pathbuf + ol))) = (uint8_t)(0));
    } else {
      (*(uint8_t*)((uintptr_t)(pathbuf)) = (uint8_t)(97));
      (*(uint8_t*)((uintptr_t)((pathbuf + 1))) = (uint8_t)(46));
      (*(uint8_t*)((uintptr_t)((pathbuf + 2))) = (uint8_t)(111));
      (*(uint8_t*)((uintptr_t)((pathbuf + 3))) = (uint8_t)(117));
      (*(uint8_t*)((uintptr_t)((pathbuf + 4))) = (uint8_t)(116));
      (*(uint8_t*)((uintptr_t)((pathbuf + 5))) = (uint8_t)(0));
    }
    write_macho(pathbuf, main_lbl, a64_gl_name, arr_len(a64_gl_name));
  }
  if ((mode == 3)) {
    if ((flag_incr == 0)) {
      if (flag_permod) {
        x64_mod_init();
        mk = arr_len(diag_file_starts);
        i = 0;
        while ((i < mk)) {
          emit_module_x86_64(i);
          i = (i + 1);
        }
        bo_resolve_calls_x64();
        start_lbl = x64_emit_start_stub();
      } else {
        start_lbl = emit_all_x86_64();
      }
    }
    elf_sdata_size = 0;
    i = 0;
    while ((i < arr_len(x64_str_tbl))) {
      fp = arr_get(x64_str_tbl, i);
      fs = unpack_s(fp);
      fl = unpack_l(fp);
      elf_sdata_off = arr_push(elf_sdata_off, elf_sdata_size);
      long long sj;
      sj = 0;
      while ((sj < fl)) {
        (*(uint8_t*)((uintptr_t)((elf_sdata + elf_sdata_size))) = (uint8_t)(((long long)(*(uint8_t*)((uintptr_t)(((x64_sbuf + fs) + sj)))))));
        elf_sdata_size = (elf_sdata_size + 1);
        sj = (sj + 1);
      }
      (*(uint8_t*)((uintptr_t)((elf_sdata + elf_sdata_size))) = (uint8_t)(0));
      elf_sdata_size = (elf_sdata_size + 1);
      i = (i + 1);
    }
    i = 0;
    while ((i < arr_len(x64_flt_tbl))) {
      fp = arr_get(x64_flt_tbl, i);
      fs = unpack_s(fp);
      elf_flt_data = arr_push(elf_flt_data, mo_atof((x64_sbuf + fs)));
      i = (i + 1);
    }
    if ((out_name != 0)) {
      long long ol;
      ol = copy_cstr(pathbuf, out_name);
      (*(uint8_t*)((uintptr_t)((pathbuf + ol))) = (uint8_t)(0));
    } else {
      (*(uint8_t*)((uintptr_t)(pathbuf)) = (uint8_t)(97));
      (*(uint8_t*)((uintptr_t)((pathbuf + 1))) = (uint8_t)(46));
      (*(uint8_t*)((uintptr_t)((pathbuf + 2))) = (uint8_t)(111));
      (*(uint8_t*)((uintptr_t)((pathbuf + 3))) = (uint8_t)(117));
      (*(uint8_t*)((uintptr_t)((pathbuf + 4))) = (uint8_t)(116));
      (*(uint8_t*)((uintptr_t)((pathbuf + 5))) = (uint8_t)(0));
    }
    write_elf(pathbuf, start_lbl, x64_gl_name, arr_len(x64_gl_name));
  }
  if ((mode == 1)) {
    emit_all_arm64();
  }
  if ((mode == 0)) {
    emit_all();
  }
  diag_summary();
  cache_save();
  return 0;
  return 0;
}

int main(int argc, char** argv) {
  _bpp_argc = argc;
  _bpp_argv = argv;
  main_bpp();
  return 0;
}
