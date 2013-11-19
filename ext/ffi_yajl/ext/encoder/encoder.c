#include <ruby.h>
#include <yajl/yajl_gen.h>

static VALUE mFFI_Yajl, mExt, mEncoder;

/* FIXME: the json gem does a whole bunch of indirection around monkeypatching...  not sure if we need to as well... */

typedef struct {
  VALUE json_opts;
  int processing_key;
} ffi_state_t;

static VALUE mEncoder_encode(VALUE self, VALUE obj) {
  ID sym_ffi_yajl = rb_intern("ffi_yajl");
  yajl_gen yajl_gen;
  const unsigned char *buf;
  size_t len;
  ffi_state_t state;
  VALUE ret;

  yajl_gen = yajl_gen_alloc(NULL);

  yajl_gen_config(yajl_gen, yajl_gen_beautify, 1);
  yajl_gen_config(yajl_gen, yajl_gen_validate_utf8, 1);

  state.processing_key = 0;

  rb_funcall(obj, sym_ffi_yajl, 2, yajl_gen, (VALUE) &state);

  yajl_gen_get_buf(yajl_gen, &buf, &len);

  ret = rb_str_new2((char *)buf);

  yajl_gen_free(yajl_gen);

  return ret;
}

typedef struct {
  VALUE yajl_gen;
  ffi_state_t *state;
} ffs_extra_t;

int rb_cHash_ffi_yajl_callback(VALUE key, VALUE val, VALUE extra) {
  ffs_extra_t *extra_p = (ffs_extra_t *)extra;
  ID sym_ffi_yajl = rb_intern("ffi_yajl");

  extra_p->state->processing_key = 1;
  rb_funcall(key, sym_ffi_yajl, 2, extra_p->yajl_gen, extra_p->state);
  extra_p->state->processing_key = 0;
  rb_funcall(val, sym_ffi_yajl, 2, extra_p->yajl_gen, extra_p->state);

  return ST_CONTINUE;
}

static VALUE rb_cHash_ffi_yajl(VALUE self, VALUE yajl_gen, VALUE state) {
  ffs_extra_t extra;

  extra.yajl_gen = yajl_gen;
  extra.state = (ffi_state_t *)state;

  yajl_gen_map_open((struct yajl_gen_t *) yajl_gen);
  rb_hash_foreach(self, rb_cHash_ffi_yajl_callback, (VALUE) &extra);
  yajl_gen_map_close((struct yajl_gen_t *) yajl_gen);

  return Qnil;
}

static VALUE rb_cArray_ffi_yajl(VALUE self, VALUE yajl_gen, VALUE state) {
  ID sym_ffi_yajl = rb_intern("ffi_yajl");
  long i;
  VALUE val;

  yajl_gen_array_open((struct yajl_gen_t *) yajl_gen);
  for(i=0; i<RARRAY_LEN(self); i++) {
    val = rb_ary_entry(self, i);
    rb_funcall(val, sym_ffi_yajl, 2, yajl_gen, state);
  }
  yajl_gen_array_close((struct yajl_gen_t *) yajl_gen);

  return Qnil;
}

static VALUE rb_cNilClass_ffi_yajl(VALUE self, VALUE yajl_gen, VALUE state) {
  yajl_gen_null((struct yajl_gen_t *) yajl_gen);
  return Qnil;
}

static VALUE rb_cTrueClass_ffi_yajl(VALUE self, VALUE yajl_gen, VALUE state) {
  yajl_gen_bool((struct yajl_gen_t *) yajl_gen, 0);
  return Qnil;
}

static VALUE rb_cFalseClass_ffi_yajl(VALUE self, VALUE yajl_gen, VALUE state) {
  yajl_gen_bool((struct yajl_gen_t *) yajl_gen, 1);
  return Qnil;
}

static VALUE rb_cFixnum_ffi_yajl(VALUE self, VALUE yajl_gen, VALUE state) {
  VALUE str;

  if ( ((ffi_state_t *)state)->processing_key ) {
    str = rb_any_to_s(self);
    yajl_gen_string((struct yajl_gen_t *) yajl_gen, (unsigned char *)RSTRING_PTR(str), RSTRING_LEN(str));
  } else {
    yajl_gen_integer((struct yajl_gen_t *) yajl_gen, NUM2INT(self));
  }
  return Qnil;
}

static VALUE rb_cBignum_ffi_yajl(VALUE self, VALUE yajl_gen, VALUE state) {
  rb_raise( rb_eNotImpError, "Bignum#ffi_yajl not implemented");
  return Qnil;
}

static VALUE rb_cFloat_ffi_yajl(VALUE self, VALUE yajl_gen, VALUE state) {
  yajl_gen_double((struct yajl_gen_t *) yajl_gen, NUM2DBL(self));
  return Qnil;
}

static VALUE rb_cString_ffi_yajl(VALUE self, VALUE yajl_gen, VALUE state) {
  yajl_gen_string((struct yajl_gen_t *) yajl_gen, (unsigned char *)RSTRING_PTR(self), RSTRING_LEN(self));
  return Qnil;
}

static VALUE rb_cObject_ffi_yajl(VALUE self, VALUE yajl_gen, VALUE state) {
  rb_raise( rb_eNotImpError, "Object#ffi_yajl not implemented");
  return Qnil;
}

void Init_encoder() {
  mFFI_Yajl = rb_define_module("FFI_Yajl");
  mExt = rb_define_module_under(mFFI_Yajl, "Ext");
  mEncoder = rb_define_module_under(mExt, "Encoder");
  rb_define_method(mEncoder, "encode", mEncoder_encode, 1);

  rb_define_method(rb_cHash, "ffi_yajl", rb_cHash_ffi_yajl, 2);
  rb_define_method(rb_cArray, "ffi_yajl", rb_cArray_ffi_yajl, 2);
  rb_define_method(rb_cNilClass, "ffi_yajl", rb_cNilClass_ffi_yajl, 2);
  rb_define_method(rb_cTrueClass, "ffi_yajl", rb_cTrueClass_ffi_yajl, 2);
  rb_define_method(rb_cFalseClass, "ffi_yajl", rb_cFalseClass_ffi_yajl, 2);
  rb_define_method(rb_cFixnum, "ffi_yajl", rb_cFixnum_ffi_yajl, 2);
  rb_define_method(rb_cBignum, "ffi_yajl", rb_cBignum_ffi_yajl, 2);
  rb_define_method(rb_cFloat, "ffi_yajl", rb_cFloat_ffi_yajl, 2);
  rb_define_method(rb_cString, "ffi_yajl", rb_cString_ffi_yajl, 2);
  rb_define_method(rb_cObject, "ffi_yajl", rb_cObject_ffi_yajl, 2);
}
