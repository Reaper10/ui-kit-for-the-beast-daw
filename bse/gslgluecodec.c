/* GSL - Generic Sound Layer
 * Copyright (C) 2002 Tim Janik
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General
 * Public License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place, Suite 330,
 * Boston, MA 02111-1307, USA.
 */
#undef  G_LOG_DOMAIN
#define	G_LOG_DOMAIN	"GslGlueCodec"
#include "gslgluecodec.h"

#include <string.h>
#include "gslcommon.h"

typedef struct {
  GslGlueContext       context;
  GslGlueCodecHandleIO io_handler;
  gpointer             user_data;
  GDestroyNotify       destroy;
  GScanner            *scanner;
} CodecContext;

typedef struct {
  GslGlueCodec *codec;
  gulong        proxy;
  gchar        *signal;
  gulong        id;
} CodecSignal;


/* --- prototypes --- */
static GslGlueEnum*     encode_describe_enum                (GslGlueContext *context,
                                                             const gchar    *enum_name);
static GslGlueIFace*    encode_describe_iface               (GslGlueContext *context,
                                                             const gchar    *iface);
static GslGlueProp*     encode_describe_prop                (GslGlueContext *context,
                                                             gulong          proxy,
                                                             const gchar    *prop_name);
static GslGlueProc*     encode_describe_proc                (GslGlueContext *context,
                                                             const gchar    *proc_name);
static gchar**          encode_list_proc_names              (GslGlueContext *context);
static gchar**          encode_list_method_names            (GslGlueContext *context,
                                                             const gchar    *iface_name);
static gchar*           encode_base_iface                   (GslGlueContext *context);
static gchar**          encode_iface_children               (GslGlueContext *context,
                                                             const gchar    *iface_name);
static gchar*           encode_proxy_iface                  (GslGlueContext *context,
                                                             gulong          proxy);
static GslGlueValue*    encode_exec_proc                    (GslGlueContext *context,
                                                             GslGlueCall    *proc_call);
static gboolean         encode_signal_connection            (GslGlueContext *context,
                                                             const gchar    *signal,
                                                             gulong          proxy,
                                                             gboolean        enable_connection);
static GslGlueValue*    encode_client_msg                   (GslGlueContext *context,
                                                             const gchar    *msg,
                                                             GslGlueValue   *value);
static void		gstring_add_signal		    (GString	    *gstring,
							     const gchar    *signal,
							     gulong          proxy,
							     GslGlueSeq     *args,
							     gboolean        connected);


/* --- variables --- */
static const GScannerConfig codec_scanner_config = {
  (
   " \t\r\n"
   )                    /* cset_skip_characters */,
  (
   G_CSET_a_2_z
   "_"
   G_CSET_A_2_Z
   )                    /* cset_identifier_first */,
  (
   G_CSET_a_2_z
   "_"
   G_CSET_A_2_Z
   G_CSET_DIGITS
   )                    /* cset_identifier_nth */,
  ( "#\n" )             /* cpair_comment_single */,
  
  FALSE                 /* case_sensitive */,
  
  TRUE                  /* skip_comment_multi */,
  TRUE                  /* skip_comment_single */,
  TRUE                  /* scan_comment_multi */,
  TRUE                  /* scan_identifier */,
  FALSE                 /* scan_identifier_1char */,
  TRUE                  /* scan_identifier_NULL */,
  TRUE                  /* scan_symbols */,
  FALSE                 /* scan_binary */,
  TRUE                  /* scan_octal */,
  TRUE                  /* scan_float */,
  TRUE                  /* scan_hex */,
  FALSE                 /* scan_hex_dollar */,
  TRUE                  /* scan_string_sq */,
  TRUE                  /* scan_string_dq */,
  TRUE                  /* numbers_2_int */,
  FALSE                 /* int_2_float */,
  FALSE                 /* identifier_2_string */,
  TRUE                  /* char_2_token */,
  FALSE                 /* symbol_2_token */,
  FALSE                 /* scope_0_fallback */,
};


/* --- functions --- */
GslGlueCodec*
gsl_glue_codec_new (GslGlueContext       *context,
		    GslGlueCodecSendEvent send_event,
		    GslGlueCodecClientMsg client_msg)
{
  GslGlueCodec *c;
  
  g_return_val_if_fail (context != NULL, NULL);
  g_return_val_if_fail (send_event != NULL, NULL);
  
  c = g_new0 (GslGlueCodec, 1);
  c->user_data = NULL;
  c->context = context;
  c->destroy = NULL;
  c->send_event = send_event;
  c->client_msg = client_msg;
  c->scanner = g_scanner_new (&codec_scanner_config);
  c->scanner->input_name = "SerializedCommand";
  
  return c;
}

void
gsl_glue_codec_set_user_data (GslGlueCodec  *codec,
			      gpointer       user_data,
			      GDestroyNotify destroy)
{
  GDestroyNotify odestroy;
  gpointer ouser_data;
  
  g_return_if_fail (codec != NULL);
  
  odestroy = codec->destroy;
  ouser_data = codec->user_data;
  codec->user_data = user_data;
  codec->destroy = destroy;
  if (odestroy)
    odestroy (ouser_data);
}

void
gsl_glue_codec_destroy (GslGlueCodec *codec)
{
  GDestroyNotify destroy;
  gpointer user_data;
  
  g_return_if_fail (codec != NULL);
  
  destroy = codec->destroy;
  user_data = codec->user_data;
  gsl_glue_context_push (codec->context);
  while (codec->signals)
    {
      CodecSignal *sig = codec->signals->data;
      guint id = sig->id;
      
      sig->id = 0;
      gsl_glue_signal_disconnect (sig->signal, sig->proxy, id);
    }
  gsl_glue_context_pop ();
  g_scanner_destroy (codec->scanner);
  g_free (codec);
  
  if (destroy)
    destroy (user_data);
}

GslGlueContext*
gsl_glue_codec_context (GslGlueCodecHandleIO handle_io,
			gpointer             user_data,
			GDestroyNotify       destroy)
{
  static const GslGlueContextTable encoder_vtable = {
    encode_describe_enum,
    encode_describe_iface,
    encode_describe_prop,
    encode_describe_proc,
    encode_list_proc_names,
    encode_list_method_names,
    encode_base_iface,
    encode_iface_children,
    encode_proxy_iface,
    encode_exec_proc,
    encode_signal_connection,
    encode_client_msg,
  };
  CodecContext *c;
  
  g_return_val_if_fail (handle_io != NULL, NULL);
  
  c = g_new0 (CodecContext, 1);
  gsl_glue_context_common_init (&c->context, &encoder_vtable);
  c->user_data = user_data;
  c->destroy = destroy;
  c->io_handler = handle_io;
  c->scanner = g_scanner_new (&codec_scanner_config);
  c->scanner->input_name = "SerializedResponse";
  
  return &c->context;
}

static guint
gsl_strlenv (gchar **str_array)
{
  guint i = 0;
  
  if (str_array)
    while (str_array[i])
      i++;
  
  return i;
}

static gchar**
gsl_strringtakev (GslRing *head)
{
  GslRing *node;
  gchar **str_array;
  guint i;
  
  if (!head)
    return NULL;
  
  i = gsl_ring_length (head);
  str_array = g_new (gchar*, i + 1);
  i = 0;
  for (node = head; node; node = gsl_ring_walk (head, node))
    str_array[i++] = node->data;
  str_array[i] = NULL;
  
  return str_array;
}

static void
g_string_add_escstr (GString     *gstring,
                     const gchar *string)
{
  g_return_if_fail (gstring != NULL);
  
  if (!string)
    g_string_append (gstring, "NULL");
  else
    {
      gchar *esc;
      esc = g_strescape (string, NULL);
      g_string_append_c (gstring, '"');
      g_string_append (gstring, esc);
      g_string_append_c (gstring, '"');
      g_free (esc);
    }
}

static GScanner*
encoder_setup_scanner (GslGlueContext *context,
		       const gchar    *input)
{
  GScanner *scanner = ((CodecContext*) context)->scanner;
  
  /* we (ab-)use scanner->user_data as expected_token field */
  scanner->user_data = NULL;
  g_scanner_input_text (scanner, input, strlen (input));
  
  return scanner;
}

static void
scanner_cleanup (GScanner    *scanner,
                 gchar       *input,
                 const gchar *floc)
{
  if (scanner->user_data)
    {
      g_printerr ("%s: while processing input: %s\n", floc, input); // FIXME
      g_scanner_unexp_token (scanner, (GTokenType) scanner->user_data, NULL, NULL, NULL, "aborting...", TRUE);
    }
  g_scanner_input_text (scanner, NULL, 0);
  g_free (input);
}

static void G_GNUC_PRINTF (2,3)
     scanner_error (GScanner    *scanner,
		    const gchar *format,
		    ...)
{
  va_list args;
  gchar *string;
  
  va_start (args, format);
  string = g_strdup_vprintf (format, args);
  va_end (args);
  
  if (scanner->parse_errors < scanner->max_parse_errors)
    g_scanner_error (scanner, "%s", string);
  
  g_free (string);
}

#define checkok(scanner)   (((GScanner*) (scanner))->user_data == NULL)
#define checkok_or_return(scanner, retval)   G_STMT_START{ \
  GScanner *__s = (scanner); \
  if (!checkok (__s)) \
    return (retval); \
}G_STMT_END
#define parse_or_return(scanner, token, retval)  G_STMT_START{ \
  GScanner *__s = (scanner); guint _t = (token); \
  if (!checkok (__s)) return (retval); \
  if (g_scanner_get_next_token (__s) != _t) { \
    __s->user_data = (gpointer) _t; \
    return (retval); \
  } \
}G_STMT_END
#define peek_or_return(scanner, token, retval)   G_STMT_START{ \
  GScanner *__s = (scanner); guint _t = (token); \
  if (!checkok (__s)) return (retval); \
  if (g_scanner_peek_next_token (__s) != _t) { \
    g_scanner_get_next_token (__s); /* advance position for error-handler */ \
    __s->user_data = (gpointer) _t; \
    return (retval); \
  } \
}G_STMT_END
#define g_string_printfa        g_string_append_printf
#define bad_parse		gsl_glue_value_inval

static gchar*
parse_string (GScanner *scanner)
{
  if (g_scanner_peek_next_token (scanner) == G_TOKEN_IDENTIFIER_NULL)
    {
      g_scanner_get_next_token (scanner);
      return NULL;
    }
  else
    {
      parse_or_return (scanner, G_TOKEN_STRING, NULL);
      return g_strdup (scanner->value.v_string);
    }
}

static gchar**
parse_string_array (GScanner *scanner)
{
  GslRing *ring = NULL;
  gchar **strings;
  
  parse_or_return (scanner, '(', NULL);
  while (g_scanner_peek_next_token (scanner) == G_TOKEN_STRING)
    {
      g_scanner_get_next_token (scanner);
      ring = gsl_ring_append (ring, g_strdup (scanner->value.v_string));
    }
  strings = gsl_strringtakev (ring);
  gsl_ring_free (ring);
  parse_or_return (scanner, ')', strings);
  return strings;
}

static void
g_string_add_string_array (GString *gstring,
                           gchar  **strv)
{
  g_string_append_c (gstring, '(');
  if (strv)
    while (strv[0])
      {
        g_string_append_c (gstring, ' ');
        g_string_add_escstr (gstring, strv[0]);
        strv++;
      }
  g_string_append_c (gstring, ')');
}

static gboolean
scanner_check_negate (GScanner *scanner)
{
  if (g_scanner_peek_next_token (scanner) == '-')
    {
      g_scanner_get_next_token (scanner);
      return TRUE;
    }
  else
    return FALSE;
}

static void
g_string_add_param (GString      *gstring,
                    GslGlueParam *param)
{
  g_string_append_c (gstring, '(');
  g_string_printfa (gstring, "%u ", param->glue_type);
  g_string_add_escstr (gstring, param->any.name);
  g_string_append_c (gstring, ' ');
  switch (param->glue_type)
    {
    case GSL_GLUE_TYPE_NONE:
      break;
    case GSL_GLUE_TYPE_BOOL:
      g_string_printfa (gstring, "%u ", param->pbool.dflt);
      break;
    case GSL_GLUE_TYPE_IRANGE:
      g_string_printfa (gstring, "%d ", param->irange.dflt);
      g_string_printfa (gstring, "%d ", param->irange.min);
      g_string_printfa (gstring, "%d ", param->irange.max);
      g_string_printfa (gstring, "%d ", param->irange.stepping);
      break;
    case GSL_GLUE_TYPE_FRANGE:
      g_string_printfa (gstring, "%.30g ", param->frange.dflt);
      g_string_printfa (gstring, "%.30g ", param->frange.min);
      g_string_printfa (gstring, "%.30g ", param->frange.max);
      g_string_printfa (gstring, "%.30g ", param->frange.stepping);
      break;
    case GSL_GLUE_TYPE_STRING:
      g_string_add_escstr (gstring, param->string.dflt);
      break;
    case GSL_GLUE_TYPE_ENUM:
      g_string_add_escstr (gstring, param->penum.enum_name);
      g_string_printfa (gstring, "%u ", param->penum.dflt);
      break;
    case GSL_GLUE_TYPE_PROXY:
      g_string_add_escstr (gstring, param->proxy.iface_name);
      break;
    case GSL_GLUE_TYPE_SEQ:
#if 0
      g_string_add_param (gstring, param->seq.elements);
#endif
      break;
    case GSL_GLUE_TYPE_REC:
#if 0
      g_string_printfa (gstring, "%u ", param->rec.n_fields);
      for (i = 0; i < n_fields; i++)
        g_string_add_param (gstring, param->rec.fields + i);
#endif
      break;
    }
  g_string_append_c (gstring, ')');
}

static GslGlueParam*
parse_param (GScanner     *scanner,
             gboolean      need_name)
{
  GslGlueParam *param;
  gchar *tmp, *param_name;
  guint i;
  
  parse_or_return (scanner, '(', NULL);
  parse_or_return (scanner, G_TOKEN_INT, NULL);
  i = scanner->value.v_int;
  if (!need_name && g_scanner_peek_next_token (scanner) == G_TOKEN_IDENTIFIER_NULL)
    g_scanner_get_next_token (scanner); /* eat NULL */
  else
    parse_or_return (scanner, G_TOKEN_STRING, NULL);
  tmp = scanner->token == G_TOKEN_STRING ? scanner->value.v_string : NULL;
  if (i > GSL_GLUE_TYPE_LAST)
    {
      scanner_error (scanner, "invalid glue type id `%u' for parameter \"%s\"",
                     i, tmp ? tmp : "");
      return NULL;
    }
  param_name = g_strdup (tmp);
  gsl_glue_gc_add (param_name, g_free);
  switch (i)
    {
      gboolean negate, si2f;
      gint idflt, imin, imax, istep;
      gdouble fdflt, fmin, fmax, fstep;
    case GSL_GLUE_TYPE_NONE:
      param = _gsl_glue_param_inval ();
      break;
    case GSL_GLUE_TYPE_BOOL:
      parse_or_return (scanner, G_TOKEN_INT, NULL);
      param = gsl_glue_param_bool (param_name, scanner->value.v_int != 0);
      break;
    case GSL_GLUE_TYPE_IRANGE:
      negate = scanner_check_negate (scanner);
      parse_or_return (scanner, G_TOKEN_INT, NULL);
      idflt = scanner->value.v_int;
      if (negate)
        idflt = -idflt;
      negate = scanner_check_negate (scanner);
      parse_or_return (scanner, G_TOKEN_INT, NULL);
      imin = scanner->value.v_int;
      if (negate)
        imin = -imin;
      negate = scanner_check_negate (scanner);
      parse_or_return (scanner, G_TOKEN_INT, NULL);
      imax = scanner->value.v_int;
      if (negate)
        imax = -imax;
      negate = scanner_check_negate (scanner);
      parse_or_return (scanner, G_TOKEN_INT, NULL);
      istep = scanner->value.v_int;
      if (negate)
        istep = -istep;
      /* validate */
      imax = MAX (imin, imax);
      idflt = CLAMP (idflt, imin, imax);
      param = gsl_glue_param_irange (param_name, idflt, imin, imax, istep);
      break;
    case GSL_GLUE_TYPE_FRANGE:
      si2f = scanner->config->int_2_float;
      scanner->config->int_2_float = TRUE;
      negate = scanner_check_negate (scanner);
      parse_or_return (scanner, G_TOKEN_FLOAT, NULL);
      fdflt = negate ? -scanner->value.v_float : scanner->value.v_float;
      negate = scanner_check_negate (scanner);
      parse_or_return (scanner, G_TOKEN_FLOAT, NULL);
      fmin = negate ? -scanner->value.v_float : scanner->value.v_float;
      negate = scanner_check_negate (scanner);
      parse_or_return (scanner, G_TOKEN_FLOAT, NULL);
      fmax = negate ? -scanner->value.v_float : scanner->value.v_float;
      negate = scanner_check_negate (scanner);
      parse_or_return (scanner, G_TOKEN_FLOAT, NULL);
      fstep = negate ? -scanner->value.v_float : scanner->value.v_float;
      /* validate */
      fmax = MAX (fmin, fmax);
      fdflt = CLAMP (fdflt, fmin, fmax);
      param = gsl_glue_param_frange (param_name, fdflt, fmin, fmax, fstep);
      scanner->config->int_2_float = si2f;
      break;
    case GSL_GLUE_TYPE_STRING:
      tmp = parse_string (scanner);
      param = gsl_glue_param_string (param_name, tmp);
      g_free (tmp);
      checkok_or_return (scanner, NULL);
      break;
    case GSL_GLUE_TYPE_ENUM:
      parse_or_return (scanner, G_TOKEN_STRING, NULL);
      param = gsl_glue_param_enum (param_name, scanner->value.v_string, 0);
      parse_or_return (scanner, G_TOKEN_INT, NULL);
      param->penum.dflt = scanner->value.v_int;
      break;
    case GSL_GLUE_TYPE_PROXY:
      parse_or_return (scanner, G_TOKEN_STRING, NULL);
      param = gsl_glue_param_proxy (param_name, scanner->value.v_string);
      break;
    case GSL_GLUE_TYPE_SEQ:
#if 0
      param->seq.elements = construct (GslGlueParam, 1);
      parse_param (scanner, param->seq.elements, FALSE);
      checkok_or_return (scanner, FALSE);
      break;
#endif
    case GSL_GLUE_TYPE_REC:
#if 0
      parse_or_return (scanner, G_TOKEN_INT, FALSE);
      n_fields = scanner->value.v_int;
      if (n_fields < 1)
        {
          scanner_error (scanner,
                         "invalid number of fields `%u' for parameter \"%s\"", param->rec.n_fields, scanner->value.v_string);
          return FALSE;
        }
      param->rec.fields = construct (GslGlueParam, param->rec.n_fields); /* PFUI */
      for (i = 0; i < n_fields; i++)
        {
          parse_param (scanner, param->rec.fields + i, TRUE);
          checkok_or_return (scanner, FALSE);
        }
      break;
#endif
    default:
      scanner_error (scanner, "can't handle glue type id `%u' for parameter \"%s\"",
                     i, tmp ? tmp : "");
      return NULL;
    }
  gsl_glue_gc_free_now (param_name, g_free);
  parse_or_return (scanner, ')', NULL);
  
  return param;
}

static void
g_string_add_value (GString      *gstring,
                    GslGlueValue *value)
{
  g_string_append_c (gstring, '(');
  g_string_printfa (gstring, "%u ", value->glue_type);
  switch (value->glue_type)
    {
      guint i;
    case GSL_GLUE_TYPE_NONE:
      break;
    case GSL_GLUE_TYPE_BOOL:
      g_string_printfa (gstring, "%u ", value->value.v_bool);
      break;
    case GSL_GLUE_TYPE_IRANGE:
      g_string_printfa (gstring, "%d ", value->value.v_int);
      break;
    case GSL_GLUE_TYPE_FRANGE:
      g_string_printfa (gstring, "%.30g ", value->value.v_float);
      break;
    case GSL_GLUE_TYPE_STRING:
      g_string_add_escstr (gstring, value->value.v_string);
      break;
    case GSL_GLUE_TYPE_ENUM:
      g_string_add_escstr (gstring, value->value.v_enum.name);
      g_string_printfa (gstring, " %u ", value->value.v_enum.index);
      break;
    case GSL_GLUE_TYPE_PROXY:
      g_string_printfa (gstring, "%lu ", value->value.v_proxy);
      break;
    case GSL_GLUE_TYPE_SEQ:
      if (value->value.v_seq)
        {
#if 0
          g_string_printfa (gstring, "%u ", value.value.v_seq->element_type);
#endif
          for (i = 0; i < value->value.v_seq->n_elements; i++)
            g_string_add_value (gstring, &value->value.v_seq->elements[i]);
        }
      break;
    case GSL_GLUE_TYPE_REC:
      if (value->value.v_rec)
        for (i = 0; i < value->value.v_rec->n_fields; i++)
	  {
	    g_string_add_escstr (gstring, value->value.v_rec->field_names[i]);
	    g_string_add_value (gstring, &value->value.v_rec->fields[i]);
	  }
      break;
    }
  g_string_append_c (gstring, ')');
}

static GslGlueValue*
parse_value (GScanner     *scanner)
{
  GslGlueValue *value;
  guint i;
  
  parse_or_return (scanner, '(', bad_parse ());
  parse_or_return (scanner, G_TOKEN_INT, bad_parse ());
  i = scanner->value.v_int;
  if (i > GSL_GLUE_TYPE_LAST)
    {
      scanner_error (scanner, "invalid glue type id `%u' for value", i);
      return bad_parse ();
    }
  switch (i)
    {
      gboolean negate, si2f;
      gchar *enum_name;
      GslGlueSeq *seq;
      GslGlueRec *rec;
    case GSL_GLUE_TYPE_NONE:
      value = gsl_glue_value_inval ();
      break;
    case GSL_GLUE_TYPE_BOOL:
      parse_or_return (scanner, G_TOKEN_INT, bad_parse ());
      value = gsl_glue_value_bool (scanner->value.v_int != 0);
      break;
    case GSL_GLUE_TYPE_IRANGE:
      negate = scanner_check_negate (scanner);
      parse_or_return (scanner, G_TOKEN_INT, bad_parse ());
      value = gsl_glue_value_int (scanner->value.v_int);
      if (negate)
        value->value.v_int = -value->value.v_int;
      break;
    case GSL_GLUE_TYPE_FRANGE:
      si2f = scanner->config->int_2_float;
      scanner->config->int_2_float = TRUE;
      negate = scanner_check_negate (scanner);
      parse_or_return (scanner, G_TOKEN_FLOAT, bad_parse ());
      value = gsl_glue_value_float (negate ? -scanner->value.v_float : scanner->value.v_float);
      scanner->config->int_2_float = si2f;
      break;
    case GSL_GLUE_TYPE_STRING:
      value = gsl_glue_value_take_string (parse_string (scanner));
      checkok_or_return (scanner, bad_parse ());
      break;
    case GSL_GLUE_TYPE_ENUM:
      enum_name = parse_string (scanner);
      value = gsl_glue_value_enum (enum_name, 0);
      g_free (enum_name);
      checkok_or_return (scanner, bad_parse ());
      parse_or_return (scanner, G_TOKEN_INT, bad_parse ());
      value->value.v_enum.index = scanner->value.v_int;
      break;
    case GSL_GLUE_TYPE_PROXY:
      parse_or_return (scanner, G_TOKEN_INT, bad_parse ());
      value = gsl_glue_value_proxy (scanner->value.v_int);
      break;
    case GSL_GLUE_TYPE_SEQ:
#if 0
      parse_or_return (scanner, G_TOKEN_INT, FALSE);
      i = scanner->value.v_int;
      if (i < GSL_GLUE_TYPE_FIRST || i > GSL_GLUE_TYPE_LAST)
        {
          scanner_error (scanner, "invalid glue type id `%u' for value sequence", i);
          return FALSE;
        }
#endif
      seq = gsl_glue_seq ();
      value = gsl_glue_value_seq (seq);
      gsl_glue_gc_collect_seq (seq);
      
      while (g_scanner_peek_next_token (scanner) != ')')
        {
          GslGlueValue *v = parse_value (scanner);
          checkok_or_return (scanner, v);
#if 0
          if (v.glue_type != value->value.v_seq->element_type)
            {
              scanner_error (scanner, "invalid value type `%u' for sequence value", v.glue_type);
              gsl_glue_reset_value (&v);
              return FALSE;
            }
#endif
          gsl_glue_seq_append (value->value.v_seq, v);
	  gsl_glue_gc_collect_value (v);
        }
      break;
    case GSL_GLUE_TYPE_REC:
      rec = gsl_glue_rec ();
      value = gsl_glue_value_rec (rec);
      gsl_glue_gc_collect_rec (rec);
      
      while (g_scanner_peek_next_token (scanner) != ')')
        {
          GslGlueValue *v;
          gchar *name;
	  
	  parse_or_return (scanner, G_TOKEN_STRING, bad_parse ());
	  name = g_strdup (scanner->value.v_string);
          v = parse_value (scanner);
	  if (!checkok (scanner))
	    {
	      g_free (name);
	      return v;
	    }
	  gsl_glue_rec_set (value->value.v_rec, name, v);
	  gsl_glue_gc_collect_value (v);
	  g_free (name);
        }
      break;
    default:
      scanner_error (scanner, "can't handle glue type id `%u' for value", i);
      return bad_parse ();
    }
  parse_or_return (scanner, ')', bad_parse ());
  
  return value;
}

static gchar*
encoder_process_request (GslGlueContext *context,
			 GString        *gstring)
{
  gchar *response;
  
  response = ((CodecContext*) context)->io_handler (((CodecContext*) context)->user_data, gstring->str);
  g_string_free (gstring, TRUE);
  
  if (response)
    {
      /* check response */
      if (strncmp (response, ";gsl-glue-return\n", 17) != 0)
        {
          g_message ("received bad glue response: %s", response);
          g_free (response);
          response = NULL;
        }
      else
        g_memmove (response, response + 17, strlen (response) - 17 + 1);
    }
  else
    g_message ("received NULL glue response");
  
  return response;
}

static GslGlueEnum*
parse_enum (GScanner *scanner)
{
  GslGlueEnum *e;
  
  parse_or_return (scanner, '(', NULL);
  parse_or_return (scanner, G_TOKEN_STRING, NULL);
  e = _gsl_glue_enum (scanner->value.v_string);
  e->values = parse_string_array (scanner);
  checkok_or_return (scanner, e);
  e->blurbs = parse_string_array (scanner);
  checkok_or_return (scanner, e);
  e->n_values = gsl_strlenv (e->values);
  if (e->n_values < 1 || e->n_values != gsl_strlenv (e->blurbs))
    scanner_error (scanner, "enumeration \"%s\" contains invalid values or blurbs", e->enum_name);
  parse_or_return (scanner, ')', e);
  return e;
}

static gboolean
decode_describe_enum (GslGlueCodec *codec,
                      GString      *response)
{
  GScanner *scanner = codec->scanner;
  GslGlueEnum *e;
  
  parse_or_return (scanner, G_TOKEN_STRING, FALSE);
  
  e = gsl_glue_describe_enum (scanner->value.v_string);
  if (e)
    {
      g_string_append_c (response, '(');
      if (e->enum_name)
        g_string_add_escstr (response, e->enum_name);
      g_string_append_c (response, ' ');
      if (e->values)
        g_string_add_string_array (response, e->values);
      g_string_append_c (response, ' ');
      if (e->blurbs)
        g_string_add_string_array (response, e->blurbs);
      gsl_glue_gc_collect_enum (e);
      g_string_append_c (response, ')');
    }
  
  return TRUE;
}

static GslGlueEnum*
encode_describe_enum (GslGlueContext *context,
		      const gchar    *enum_name)
{
  GString *gstring = g_string_new (NULL);
  GslGlueEnum *e;
  GScanner *scanner;
  gchar *input;
  
  g_string_printfa (gstring, "(%u ", GSL_GLUE_CODEC_DESCRIBE_ENUM);
  g_string_add_escstr (gstring, enum_name);
  g_string_append_c (gstring, ')');
  
  input = encoder_process_request (context, gstring);
  scanner = encoder_setup_scanner (context, input);
  e = parse_enum (scanner);
  scanner_cleanup (scanner, input, G_STRLOC);
  
  return e;
}

static GslGlueIFace*
parse_iface (GScanner *scanner)
{
  GslGlueIFace *f;
  
  parse_or_return (scanner, '(', NULL);
  parse_or_return (scanner, G_TOKEN_STRING, NULL);
  f = _gsl_glue_iface (scanner->value.v_string);
  f->ifaces = parse_string_array (scanner);
  checkok_or_return (scanner, f);
  f->n_ifaces = gsl_strlenv (f->ifaces);
  if (f->n_ifaces < 1)
    {
      scanner_error (scanner, "interface \"%s\" has invalid ancestry", f->type_name);
      return f;
    }
  f->props = parse_string_array (scanner);
  checkok_or_return (scanner, f);
  f->n_props = gsl_strlenv (f->props);
  f->signals = parse_string_array (scanner);
  checkok_or_return (scanner, f);
  f->n_signals = gsl_strlenv (f->signals);
  parse_or_return (scanner, ')', f);
  return f;
}

static gboolean
decode_describe_iface (GslGlueCodec *codec,
                       GString      *response)
{
  GScanner *scanner = codec->scanner;
  GslGlueIFace *f;
  
  parse_or_return (scanner, G_TOKEN_STRING, FALSE);
  
  f = gsl_glue_describe_iface (scanner->value.v_string);
  if (f)
    {
      g_string_append_c (response, '(');
      if (f->type_name)
        g_string_add_escstr (response, f->type_name);
      g_string_append_c (response, ' ');
      if (f->ifaces)
        g_string_add_string_array (response, f->ifaces);
      g_string_append_c (response, ' ');
      if (f->props)
        g_string_add_string_array (response, f->props);
      g_string_append_c (response, ' ');
      if (f->signals)
        g_string_add_string_array (response, f->signals);
      gsl_glue_gc_collect_iface (f);
      g_string_append_c (response, ')');
    }
  
  return TRUE;
}

static GslGlueIFace*
encode_describe_iface (GslGlueContext *context,
		       const gchar    *iface)
{
  GString *gstring = g_string_new (NULL);
  GslGlueIFace *f;
  GScanner *scanner;
  gchar *input;
  
  g_string_printfa (gstring, "(%u ", GSL_GLUE_CODEC_DESCRIBE_IFACE);
  g_string_add_escstr (gstring, iface);
  g_string_append_c (gstring, ')');
  
  input = encoder_process_request (context, gstring);
  scanner = encoder_setup_scanner (context, input);
  f = parse_iface (scanner);
  scanner_cleanup (scanner, input, G_STRLOC);
  
  return f;
}

static GslGlueProp*
parse_prop (GScanner *scanner)
{
  GslGlueProp *p;
  
  p = _gsl_glue_prop ();
  parse_or_return (scanner, '(', p);
  p->param = parse_param (scanner, TRUE);
  gsl_glue_gc_remove (p->param, _gsl_glue_param_free);
  checkok_or_return (scanner, p);
  p->group = parse_string (scanner);
  checkok_or_return (scanner, p);
  p->pretty_name = parse_string (scanner);
  checkok_or_return (scanner, p);
  p->blurb = parse_string (scanner);
  checkok_or_return (scanner, p);
  parse_or_return (scanner, G_TOKEN_INT, p);
  p->flags = scanner->value.v_int;
  parse_or_return (scanner, ')', p);
  return p;
}

static gboolean
decode_describe_prop (GslGlueCodec *codec,
                      GString      *response)
{
  GScanner *scanner = codec->scanner;
  GslGlueProp *p;
  gulong proxy;
  
  parse_or_return (scanner, G_TOKEN_INT, FALSE);
  proxy = scanner->value.v_int;
  parse_or_return (scanner, G_TOKEN_STRING, FALSE);
  
  p = gsl_glue_describe_prop (proxy, scanner->value.v_string);
  if (p)
    {
      g_string_append_c (response, '(');
      g_string_add_param (response, p->param);
      g_string_append_c (response, ' ');
      g_string_add_escstr (response, p->group);
      g_string_append_c (response, ' ');
      g_string_add_escstr (response, p->pretty_name);
      g_string_append_c (response, ' ');
      g_string_add_escstr (response, p->blurb);
      g_string_printfa (response, " %u", p->flags);
      gsl_glue_gc_collect_prop (p);
      g_string_append_c (response, ')');
    }
  
  return TRUE;
}

static GslGlueProp*
encode_describe_prop (GslGlueContext *context,
		      gulong          proxy,
		      const gchar    *prop_name)
{
  GString *gstring = g_string_new (NULL);
  GslGlueProp *p;
  GScanner *scanner;
  gchar *input;
  
  g_string_printfa (gstring, "(%u %lu ", GSL_GLUE_CODEC_DESCRIBE_PROP, proxy);
  g_string_add_escstr (gstring, prop_name);
  g_string_append_c (gstring, ')');
  
  input = encoder_process_request (context, gstring);
  scanner = encoder_setup_scanner (context, input);
  if (g_scanner_peek_next_token (scanner) == '(')
    p = parse_prop (scanner);
  else
    p = NULL;   /* valid case */
  scanner_cleanup (scanner, input, G_STRLOC);
  
  return p;
}

static GslGlueProc*
parse_proc (GScanner *scanner)
{
  GslGlueProc *p;
  GslGlueParam *param;
  guint i, n;
  
  p = _gsl_glue_proc ();
  parse_or_return (scanner, '(', p);
  parse_or_return (scanner, G_TOKEN_STRING, p);
  p->proc_name = g_strdup (scanner->value.v_string);
  param = parse_param (scanner, FALSE);
  checkok_or_return (scanner, p);
  _gsl_glue_proc_take_ret_param (p, param);
  parse_or_return (scanner, G_TOKEN_INT, p);
  n = scanner->value.v_int;
  for (i = 0; i < n; i++)
    {
      _gsl_glue_proc_take_param (p, parse_param (scanner, TRUE));
      checkok_or_return (scanner, p);
    }
  parse_or_return (scanner, ')', p);
  return p;
}

static gboolean
decode_describe_proc (GslGlueCodec *codec,
                      GString      *response)
{
  GScanner *scanner = codec->scanner;
  GslGlueProc *p;
  
  parse_or_return (scanner, G_TOKEN_STRING, FALSE);
  
  p = gsl_glue_describe_proc (scanner->value.v_string);
  if (p)
    {
      g_string_append_c (response, '(');
      if (p->proc_name)
        {
          guint i;
          
          g_string_add_escstr (response, p->proc_name);
          g_string_append_c (response, ' ');
	  g_string_add_param (response, p->ret_param);
          g_string_append_printf (response, " %u", p->n_params);
          for (i = 0; i < p->n_params; i++)
            {
              g_string_append_c (response, ' ');
              g_string_add_param (response, p->params[i]);
            }
        }
      gsl_glue_gc_collect_proc (p);
      g_string_append_c (response, ')');
    }
  
  return TRUE;
}

static GslGlueProc*
encode_describe_proc (GslGlueContext *context,
		      const gchar    *proc_name)
{
  GString *gstring = g_string_new (NULL);
  GScanner *scanner;
  GslGlueProc *p;
  gchar *input;
  
  g_string_printfa (gstring, "(%u ", GSL_GLUE_CODEC_DESCRIBE_PROC);
  g_string_add_escstr (gstring, proc_name);
  g_string_append_c (gstring, ')');
  
  input = encoder_process_request (context, gstring);
  scanner = encoder_setup_scanner (context, input);
  p = parse_proc (scanner);
  scanner_cleanup (scanner, input, G_STRLOC);
  
  return p;
}

static gboolean
decode_list_proc_names (GslGlueCodec *codec,
                        GString      *response)
{
  // GScanner *scanner = codec->scanner;
  gchar **names;
  
  names = gsl_glue_list_proc_names ();
  if (names)
    g_string_add_string_array (response, names);
  
  return TRUE;
}

static gchar**
encode_list_proc_names (GslGlueContext *context)
{
  GString *gstring = g_string_new (NULL);
  gchar **names;
  GScanner *scanner;
  gchar *input;
  
  g_string_printfa (gstring, "(%u)", GSL_GLUE_CODEC_LIST_PROC_NAMES);
  
  input = encoder_process_request (context, gstring);
  scanner = encoder_setup_scanner (context, input);
  names = parse_string_array (scanner);
  scanner_cleanup (scanner, input, G_STRLOC);
  
  return names;
}

static gboolean
decode_list_method_names (GslGlueCodec *codec,
                          GString      *response)
{
  GScanner *scanner = codec->scanner;
  gchar **names;
  
  parse_or_return (scanner, G_TOKEN_STRING, FALSE);
  
  names = gsl_glue_list_method_names (scanner->value.v_string);
  if (names)
    g_string_add_string_array (response, names);
  
  return TRUE;
}

static gchar**
encode_list_method_names (GslGlueContext *context,
			  const gchar    *iface_name)
{
  GString *gstring = g_string_new (NULL);
  gchar **names;
  GScanner *scanner;
  gchar *input;
  
  g_string_printfa (gstring, "(%u ", GSL_GLUE_CODEC_LIST_METHOD_NAMES);
  g_string_add_escstr (gstring, iface_name);
  g_string_append_c (gstring, ')');
  
  input = encoder_process_request (context, gstring);
  scanner = encoder_setup_scanner (context, input);
  names = parse_string_array (scanner);
  scanner_cleanup (scanner, input, G_STRLOC);
  
  return names;
}

static gboolean
decode_base_iface (GslGlueCodec *codec,
                   GString      *response)
{
  // GScanner *scanner = codec->scanner;
  gchar *name;
  
  name = gsl_glue_base_iface ();
  g_string_add_escstr (response, name);
  
  return TRUE;
}

static gchar*
encode_base_iface (GslGlueContext *context)
{
  GString *gstring = g_string_new (NULL);
  gchar *name;
  GScanner *scanner;
  gchar *input;
  
  g_string_printfa (gstring, "(%u)", GSL_GLUE_CODEC_BASE_IFACE);
  
  input = encoder_process_request (context, gstring);
  scanner = encoder_setup_scanner (context, input);
  name = parse_string (scanner);
  scanner_cleanup (scanner, input, G_STRLOC);
  
  return name;
}

static gboolean
decode_iface_children (GslGlueCodec *codec,
                       GString      *response)
{
  GScanner *scanner = codec->scanner;
  gchar **names;
  
  parse_or_return (scanner, G_TOKEN_STRING, FALSE);
  
  names = gsl_glue_iface_children (scanner->value.v_string);
  if (names)
    g_string_add_string_array (response, names);
  
  return TRUE;
}

static gchar**
encode_iface_children (GslGlueContext *context,
		       const gchar    *iface_name)
{
  GString *gstring = g_string_new (NULL);
  gchar **names;
  GScanner *scanner;
  gchar *input;
  
  g_string_printfa (gstring, "(%u ", GSL_GLUE_CODEC_IFACE_CHILDREN);
  g_string_add_escstr (gstring, iface_name);
  g_string_append_c (gstring, ')');
  
  input = encoder_process_request (context, gstring);
  scanner = encoder_setup_scanner (context, input);
  names = parse_string_array (scanner);
  scanner_cleanup (scanner, input, G_STRLOC);
  
  return names;
}

static gboolean
decode_proxy_iface (GslGlueCodec *codec,
                    GString      *response)
{
  GScanner *scanner = codec->scanner;
  gchar *name;
  
  parse_or_return (scanner, G_TOKEN_INT, FALSE);
  name = gsl_glue_proxy_iface (scanner->value.v_int);
  g_string_add_escstr (response, name);
  
  return TRUE;
}

static gchar*
encode_proxy_iface (GslGlueContext *context,
		    gulong          proxy)
{
  GString *gstring = g_string_new (NULL);
  gchar *name;
  GScanner *scanner;
  gchar *input;
  
  g_string_printfa (gstring, "(%u %lu)", GSL_GLUE_CODEC_PROXY_IFACE, proxy);
  
  input = encoder_process_request (context, gstring);
  scanner = encoder_setup_scanner (context, input);
  name = parse_string (scanner);
  scanner_cleanup (scanner, input, G_STRLOC);
  
  return name;
}

static gboolean
decode_exec_proc (GslGlueCodec *codec,
                  GString      *response)
{
  GScanner *scanner = codec->scanner;
  GslGlueValue *value;
  gchar *pname;
  
  parse_or_return (scanner, G_TOKEN_STRING, FALSE);
  pname = g_strdup (scanner->value.v_string);
  value = parse_value (scanner);
  if (checkok (scanner) && value->glue_type == GSL_GLUE_TYPE_SEQ)
    {
      GslGlueCall *call = _gsl_glue_call_proc_seq (pname, value->value.v_seq);
      gsl_glue_call_exec (call);
      g_string_add_value (response, call->ret_value);
      gsl_glue_gc_collect_call (call);
      gsl_glue_gc_collect_value (value);
    }
  else
    {
      gsl_glue_gc_collect_value (value);
      value = gsl_glue_value_inval ();
      g_string_add_value (response, value);
      gsl_glue_gc_collect_value (value);
    }
  g_free (pname);
  
  return TRUE;
}

static GslGlueValue*
encode_exec_proc (GslGlueContext *context,
		  GslGlueCall    *call)
{
  GString *gstring = g_string_new (NULL);
  GslGlueValue *value;
  GScanner *scanner;
  gchar *input;
  
  g_string_printfa (gstring, "(%u ", GSL_GLUE_CODEC_EXEC);
  g_string_add_escstr (gstring, call->proc_name);
  g_string_append_c (gstring, ' ');
  
  value = gsl_glue_value_seq (call->params);
  g_string_add_value (gstring, value);
  gsl_glue_gc_collect_value (value);
  
  g_string_append_c (gstring, ')');
  
  input = encoder_process_request (context, gstring);
  scanner = encoder_setup_scanner (context, input);
  value = parse_value (scanner);
  scanner_cleanup (scanner, input, G_STRLOC);
  
  return value;
}

static CodecSignal*
find_signal (GslGlueCodec *codec,
	     gulong        proxy,
	     const gchar  *signal)
{
  GslRing *ring;
  
  for (ring = codec->signals; ring; ring = gsl_ring_walk (codec->signals, ring))
    {
      CodecSignal *sig = ring->data;
      if (proxy == sig->proxy &&
	  strcmp (signal, sig->signal) == 0)
	return sig;
    }
  return NULL;
}

static void
codec_signal_destroy (gpointer data)
{
  CodecSignal *sig = data;
  GslGlueCodec *codec = sig->codec;
  
  if (sig->id)
    {
      GString *gstring = g_string_new (NULL);
      GslGlueSeq *args = gsl_glue_seq ();
      GslGlueValue *value = gsl_glue_value_proxy (sig->proxy);
      
      gsl_glue_seq_append (args, value);
      gsl_glue_gc_collect_value (value);
      gstring_add_signal (gstring, sig->signal, sig->proxy, args, 0);
      gsl_glue_gc_collect_seq (args);
      
      codec->send_event (codec, codec->user_data, gstring->str);
      
      g_string_free (gstring, TRUE);
    }
  
  sig->codec->signals = gsl_ring_remove (sig->codec->signals, sig);
  g_free (sig->signal);
  g_free (sig);
}

static void
codec_handle_signal (gpointer          sig_data,
		     const gchar      *signal,
		     const GslGlueSeq *args)
{
  CodecSignal *sig = sig_data;
  GslGlueCodec *codec = sig->codec;
  GString *gstring = g_string_new (NULL);
  
  gstring_add_signal (gstring, sig->signal, sig->proxy, (GslGlueSeq*) args, 1);
  
  codec->send_event (codec, codec->user_data, gstring->str);
  
  g_string_free (gstring, TRUE);
}

static gboolean
decode_signal_connection (GslGlueCodec *codec,
                          GString      *response)
{
  GScanner *scanner = codec->scanner;
  gulong proxy;
  gboolean enable_connection, connected = FALSE;
  
  parse_or_return (scanner, G_TOKEN_INT, FALSE);
  proxy = scanner->value.v_int;
  parse_or_return (scanner, G_TOKEN_INT, FALSE);
  enable_connection = scanner->value.v_int > 0;
  parse_or_return (scanner, G_TOKEN_STRING, FALSE);
  peek_or_return (scanner, ')', FALSE);
  
  if (proxy)
    {
      CodecSignal *sig = find_signal (codec, proxy, scanner->value.v_string);
      
      if (!sig && !enable_connection)
	g_message ("glue signal to (proxy: %lu, signal: %s) already disabled",
		   proxy, scanner->value.v_string);
      else if (!sig && enable_connection)
	{
	  guint id;
	  sig = g_new0 (CodecSignal, 1);
	  sig->codec = codec;
	  sig->proxy = proxy;
	  sig->signal = g_strdup (scanner->value.v_string);
	  sig->id = 0;
	  codec->signals = gsl_ring_prepend (codec->signals, sig);
	  id = gsl_glue_signal_connect (sig->signal, sig->proxy, codec_handle_signal, sig, codec_signal_destroy);
	  if (id)
	    {
	      connected = TRUE;
	      sig->id = id;
	    }
	  else
	    {
	      connected = FALSE;
	      /* sig is already destroyed */
	    }
	}
      else if (sig && !enable_connection)
	gsl_glue_signal_disconnect (sig->signal, sig->proxy, sig->id);
      else /* sig && enable_connection */
	{
	  g_message ("glue signal to (proxy: %lu, signal: %s) already enabled",
		     sig->proxy, sig->signal);
	  connected = TRUE;
	}
    }
  g_string_printfa (response, "%u", connected);
  
  return TRUE;
}

static gboolean
encode_signal_connection (GslGlueContext *context,
			  const gchar    *signal,
			  gulong          proxy,
			  gboolean        enable_connection)
{
  GString *gstring = g_string_new (NULL);
  GScanner *scanner;
  gchar *input;
  gboolean connected;
  
  g_string_printfa (gstring, "(%u ", GSL_GLUE_CODEC_SIGNAL_CONNECTION);
  g_string_printfa (gstring, "%lu %u ", proxy, enable_connection != FALSE);
  g_string_add_escstr (gstring, signal);
  g_string_append_c (gstring, ')');
  
  input = encoder_process_request (context, gstring);
  scanner = encoder_setup_scanner (context, input);
  connected = g_scanner_get_next_token (scanner) == G_TOKEN_INT && scanner->value.v_int > 0;
  scanner_cleanup (scanner, input, G_STRLOC);
  
  return connected;
}

static gboolean
decode_client_msg (GslGlueCodec *codec,
                   GString      *response)
{
  GScanner *scanner = codec->scanner;
  GslGlueValue *v, *retv = NULL;
  gchar *msg;
  gboolean handled = FALSE;
  
  msg = parse_string (scanner);
  if (!checkok (scanner))
    {
      g_free (msg);
      return FALSE;
    }
  v = parse_value (scanner);
  if (!checkok (scanner))
    {
      g_free (msg);
      gsl_glue_gc_collect_value (v);
      return FALSE;
    }

  /* interception handler */
  if (codec->client_msg)
    {
      retv = codec->client_msg (codec, codec->user_data, msg, v, &handled);
      if (!handled && retv)
        gsl_glue_gc_collect_value (retv);
    }

  /* regular handler */
  if (!handled)
    retv = gsl_glue_client_msg (msg, v);

  /* return value marshalling */
  if (!retv)
    retv = gsl_glue_value_inval ();
  g_string_add_value (response, retv);
  gsl_glue_gc_collect_value (retv);

  /* cleanup */
  g_free (msg);
  gsl_glue_gc_collect_value (v);
  
  return TRUE;
}

static GslGlueValue*
encode_client_msg (GslGlueContext *context,
		   const gchar    *msg,
		   GslGlueValue   *value)
{
  GString *gstring = g_string_new (NULL);
  GslGlueValue *retval;
  GScanner *scanner;
  gchar *input;
  
  g_string_printfa (gstring, "(%u ", GSL_GLUE_CODEC_CLIENT_MSG);
  g_string_add_escstr (gstring, msg);
  g_string_append_c (gstring, ' ');
  g_string_add_value (gstring, value);
  g_string_append_c (gstring, ')');
  
  input = encoder_process_request (context, gstring);
  scanner = encoder_setup_scanner (context, input);
  retval = parse_value (scanner);
  scanner_cleanup (scanner, input, G_STRLOC);
  
  return retval;
}

static gboolean
decode_process_command (GslGlueCodec *codec,
                        GString      *response)
{
  GScanner *scanner = codec->scanner;
  
  parse_or_return (scanner, '(', FALSE);
  parse_or_return (scanner, G_TOKEN_INT, FALSE);
  switch (scanner->value.v_int)
    {
    case GSL_GLUE_CODEC_DESCRIBE_ENUM:
      decode_describe_enum (codec, response);
      break;
    case GSL_GLUE_CODEC_DESCRIBE_IFACE:
      decode_describe_iface (codec, response);
      break;
    case GSL_GLUE_CODEC_DESCRIBE_PROP:
      decode_describe_prop (codec, response);
      break;
    case GSL_GLUE_CODEC_DESCRIBE_PROC:
      decode_describe_proc (codec, response);
      break;
    case GSL_GLUE_CODEC_LIST_PROC_NAMES:
      decode_list_proc_names (codec, response);
      break;
    case GSL_GLUE_CODEC_LIST_METHOD_NAMES:
      decode_list_method_names (codec, response);
      break;
    case GSL_GLUE_CODEC_BASE_IFACE:
      decode_base_iface (codec, response);
      break;
    case GSL_GLUE_CODEC_IFACE_CHILDREN:
      decode_iface_children (codec, response);
      break;
    case GSL_GLUE_CODEC_PROXY_IFACE:
      decode_proxy_iface (codec, response);
      break;
    case GSL_GLUE_CODEC_EXEC:
      decode_exec_proc (codec, response);
      break;
    case GSL_GLUE_CODEC_SIGNAL_CONNECTION:
      decode_signal_connection (codec, response);
      break;
    case GSL_GLUE_CODEC_CLIENT_MSG:
      decode_client_msg (codec, response);
      break;
    default:
      scanner_error (scanner, "invalid command id `%lu'", scanner->value.v_int);
      return FALSE;
    }
  checkok_or_return (scanner, FALSE);
  parse_or_return (scanner, ')', FALSE);
  return TRUE;
}

gchar*
gsl_glue_codec_process (GslGlueCodec *codec,
                        const gchar  *message)
{
  static GScanner *scanner = NULL;
  GString *gstring;
  
  g_return_val_if_fail (codec != NULL, NULL);
  g_return_val_if_fail (message != NULL, NULL);
  
  gsl_glue_context_push (codec->context);
  scanner = codec->scanner;
  scanner->user_data = NULL;
  g_scanner_input_text (scanner, message, strlen (message));
  
  gstring = g_string_new (";gsl-glue-return\n");
  decode_process_command (codec, gstring);
  
  if (scanner->user_data)
    {
      g_scanner_unexp_token (scanner, (GTokenType) scanner->user_data, NULL, NULL, NULL, "aborting...", TRUE);
      g_string_assign (gstring, ";gsl-glue-parse-error");
    }
  g_scanner_input_text (scanner, NULL, 0);
  gsl_glue_context_pop ();
  
  return g_string_free (gstring, FALSE);
}

static void
gstring_add_signal (GString     *gstring,
		    const gchar *signal,
		    gulong       proxy,
		    GslGlueSeq  *args,
		    gboolean     connected)
{
  GslGlueValue *value;
  
  g_string_printfa (gstring, "(%u ", GSL_GLUE_CODEC_EVENT_SIGNAL);
  g_string_add_escstr (gstring, signal);
  g_string_printfa (gstring, " %lu ", proxy);
  
  value = gsl_glue_value_seq (args);
  g_string_add_value (gstring, value);
  gsl_glue_gc_collect_value (value);
  
  g_string_printfa (gstring, " %u)", connected);
}

static gboolean       /* returns whether there was an event */
decode_event (GslGlueContext *context,
	      gchar         **signal,
	      gulong	     *proxy_p,
	      GslGlueSeq    **args,
	      gboolean       *connected)
{
  GScanner *scanner = ((CodecContext*) context)->scanner;
  GslGlueValue *value;
  gulong proxy;
  
  parse_or_return (scanner, '(', FALSE);
  parse_or_return (scanner, G_TOKEN_INT, FALSE);
  if (scanner->value.v_int != GSL_GLUE_CODEC_EVENT_SIGNAL)
    return FALSE;
  
  parse_or_return (scanner, G_TOKEN_STRING, TRUE);
  *signal = g_strdup (scanner->value.v_string);
  parse_or_return (scanner, G_TOKEN_INT, TRUE);
  proxy = scanner->value.v_int;
  value = parse_value (scanner);
  checkok_or_return (scanner, TRUE);
  if (value->glue_type != GSL_GLUE_TYPE_SEQ)
    {
      gsl_glue_gc_collect_value (value);
      return TRUE;
    }
  else
    {
      *args = gsl_glue_seq_ref (value->value.v_seq);
      gsl_glue_gc_collect_value (value);
    }
  parse_or_return (scanner, G_TOKEN_INT, TRUE);
  *connected = scanner->value.v_int;
  parse_or_return (scanner, ')', TRUE);
  
  *proxy_p = proxy;	/* success */
  return TRUE;
}

gboolean	/* returns whether message contains an event */
gsl_glue_codec_enqueue_event (GslGlueContext *context,
			      const gchar    *message)
{
  GScanner *scanner;
  gchar *signal = NULL;
  gulong proxy = 0;
  GslGlueSeq *args = NULL;
  gboolean seen_event, connected = FALSE;
  
  g_return_val_if_fail (context != NULL, FALSE);
  g_return_val_if_fail (message != NULL, FALSE);
  
  scanner = encoder_setup_scanner (context, message);
  seen_event = decode_event (context, &signal, &proxy, &args, &connected);
  scanner_cleanup (scanner, NULL, G_STRLOC);
  
  if (proxy)
    gsl_glue_enqueue_signal_event (signal, args, !connected);
  g_free (signal);
  if (args)
    gsl_glue_seq_unref (args);
  
  return seen_event;
}

/* vim:set ts=8 sts=2 sw=2: */
