#include "read.h"

#ifdef USE_INDEX
#include "datindex.h"
#include "digest.h"
#endif

#ifdef ENGLISH
#include "r2chhtml_en.h"
#else
#include "r2chhtml.h"
#endif

#if defined PREVENTRELOAD && !defined FORCE_304_TIME
# define FORCE_304_TIME  30    /* 秒で指定 */
#endif

#if	('\xFF' != 0xFF)
#error	-funsigned-char required.
	/* このシステムでは、-funsigned-charを要求する */
#endif

#if	(defined(CHUNK_ANCHOR) && CHUNK_NUM > RES_NORMAL) 
# error "Too large CHUNK_NUM!!"
#endif

/* CHUNK_ANCHOR のコードに依存している */
#if defined(SEPARATE_CHUNK_ANCHOR) && !defined(CHUNK_ANCHOR)
#error	SEPARATE_CHUNK_ANCHOR require CHUNK_ANCHOR
#endif

#ifndef READ_KAKO
# undef READ_TEMP
#endif
#if	defined(RAWOUT_MULTI) && !(defined(RAWOUT) && defined(Katjusha_DLL_REPLY))
# undef RAWOUT_MULTI
#endif

#ifdef USE_SETTING_FILE
/*
	SETTING_R.TXTは
	---
	FORCE_304_TIME=30
	LIMIT_PM=23
	RES_NORMAL=50
	MAX_FILESIZE=512
	LINKTAGCUT=0
	---
	など。空行可。
	#と;から開始はコメント・・・というより、
	=がなかったり、マッチしなかったりしたら無視
	最後の行に改行が入ってなかったら、それも無視される(バグと書いて仕様と読む)
	
	RES_YELLOW-RES_NORMALまでは、#defineのままでいいかも。
*/

typedef struct {
	int Res_Yellow;
	int Res_RedZone;
	int Res_Imode;
	int Res_Normal;
	int Max_FileSize;	/*	こいつだけ、KByte単位	*/
	int Limit_PM;
	int Limit_AM;
#ifdef PREVENTRELOAD
	int Force_304_Time;
#endif
	int Latest_Num;
	int LinkTagCut;
} settings_t;

static const settings_t defaultSettings = {
	RES_YELLOW,
	RES_REDZONE,
	RES_IMODE,
	RES_NORMAL,
	MAX_FILESIZE / 1024,
	LIMIT_PM,
	LIMIT_AM,
#ifdef	PREVENTRELOAD
	FORCE_304_TIME,
#endif
	LATEST_NUM,
	LINKTAGCUT
};

typedef struct {
	const char *str;
	int *val;
	int len;
	/*	文字列の長さをあらかじめ数えたり、２分探索用に並べておくのは、
		拡張する時にちょっと。
		負荷が限界にきていたら考えるべし。
	*/
} settingParam_t;

static const settingParam_t defaultSettingParam[] = {
	{ "RES_YELLOW",     &((settings_t *)NULL)->Res_Yellow },
	{ "RES_REDZONE",    &((settings_t *)NULL)->Res_RedZone },
	{ "RES_IMODE",      &((settings_t *)NULL)->Res_Imode },
	{ "RES_NORMAL",     &((settings_t *)NULL)->Res_Normal },
	{ "MAX_FILESIZE",   &((settings_t *)NULL)->Max_FileSize },
	{ "LIMIT_PM",       &((settings_t *)NULL)->Limit_PM },
	{ "LIMIT_AM",       &((settings_t *)NULL)->Limit_AM },
#ifdef	PREVENTRELOAD
	{ "FORCE_304_TIME", &((settings_t *)NULL)->Force_304_Time },
#endif
	{ "LATEST_NUM",     &((settings_t *)NULL)->Latest_Num },
	{ "LINKTAGCUT",     &((settings_t *)NULL)->LinkTagCut }
};

#undef	RES_YELLOW
#define	RES_YELLOW	gv->Settings.Res_Yellow
#undef	RES_REDZONE
#define	RES_REDZONE	gv->Settings.Res_RedZone
#undef	RES_IMODE
#define	RES_IMODE	gv->Settings.Res_Imode
#undef	RES_NORMAL
#define	RES_NORMAL	gv->Settings.Res_Normal
#undef	MAX_FILESIZE
#define	MAX_FILESIZE	(gv->Settings.Max_FileSize * 1024)
#undef	LIMIT_PM
#define	LIMIT_PM	gv->Settings.Limit_PM
#undef	LIMIT_AM
#define	LIMIT_AM	gv->Settings.Limit_AM
#ifdef	PREVENTRELOAD
#undef	FORCE_304_TIME
#define	FORCE_304_TIME	gv->Settings.Force_304_Time
#endif
#undef	LATEST_NUM
#define	LATEST_NUM	gv->Settings.Latest_Num
#undef	LINKTAGCUT
#define	LINKTAGCUT	gv->Settings.LinkTagCut

#endif /* USE_SETTING_FILE */

#define arraylen(a) (sizeof a / sizeof *a)

/* global変数をラッピングw
   初期化は init_global_vars() で */
struct global_vars {
	char *cwd;

	int zz_head_request; /* !0 = HEAD request */
	char const *zz_remote_addr;
	char const *zz_remote_host;
	char const *zz_http_referer;
	char const *zz_server_name;
	char const *zz_script_name;
	int need_basehref;

	char const *zz_path_info;
	/* 0 のときは、pathは適用されていない
	   read.cgi/tech/998845501/ のときは、3になる */
	int path_depth;
	char const *zz_query_string;
	char const *zz_temp;
	char const *zz_http_user_agent;
	char const *zz_http_language;

	char const *zz_http_if_modified_since;
	apr_time_t zz_fileLastmod;

#ifdef USE_MMAP
	apr_mmap_t *zz_mmap;
	apr_file_t *zz_mmap_f;
#endif

	char zz_bs[1024];
	char zz_ky[1024];
	char zz_ls[1024];
	char zz_st[1024];
	char zz_to[1024];
	char zz_nf[1024];
	char zz_im[1024];
	char zz_parent_link[128];
	char zz_cgi_path[128];
	long nn_ky;	/* zz_kyを数字にしたもの */
#ifdef RAWOUT
	char zz_rw[1024];
#endif
#ifdef READ_KAKO
	char read_kako[256];
#endif
#ifdef	AUTO_KAKO
	int zz_dat_where;	/* 0: dat/  1: temp/  2: kako/ */
#endif
#ifdef	AUTO_LOGTYPE
	int zz_log_type;	/* 0: non-TYPE_TERI  1: TYPE_TERI */
#endif
	int nn_st, nn_to, nn_ls;

#define RANGE_MAX_RES (65535) /* レス番号より十分に大きな数 */
#define MAX_RANGE 20 
	struct range {
		int count;
		struct range_elem { 
			int from, to; 
		} array[MAX_RANGE]; 
	} nn_range;

	char *BigBuffer;
	char const *BigLine[RES_RED + 2];
	char zz_title[256];

	apr_size_t zz_fileSize;
	int lineMax;
	int out_resN;

#if defined(SEPARATE_CHUNK_ANCHOR) || defined(PREV_NEXT_ANCHOR)
	int first_line;
	int last_line;
#endif

	apr_time_t t_now;
	apr_time_exp_t tm_now;
	long currentTime;
	int isbusytime;

#ifdef RAWOUT
	int rawmode;
	int raw_lastnum, raw_lastsize; /* clientが持っているデータの番号とサイズ */
#ifdef	Katjusha_DLL_REPLY
	int zz_katjusha_raw;
#endif
#endif
#ifdef PREV_NEXT_ANCHOR
	int need_tail_comment;
#endif

#ifdef USE_SETTING_FILE
	settings_t Settings;
	settingParam_t SettingParam[arraylen(defaultSettingParam)];
#endif

	/* 関数内の静的変数抜き出し．エレガントな方法とはいえないが，
	   thread-specific 変数は面倒だし，MT-safe 化のためやむなし */
	char create_link_url_expr[128];
	char *create_link_mid_start;
	char find_kakodir_soko[256];
};

static const char KARA[] = "";

#define is_imode() (*gv->zz_im == 't')
#define is_nofirst() (*gv->zz_nf == 't')
#define is_head() (gv->zz_head_request != 0)

#define set_imode_true()	(*gv->zz_im = 't')
#define set_nofirst_true()	(*gv->zz_nf = 't')
#define set_nofirst_false()	(*gv->zz_nf = '\0')

static char const *LastChar(char const *src, char c);
static char const *GetString(char const *line, char *dst, size_t dst_size, char const *tgt);
static apr_time_t getFileLastmod(global_vars_t *gv, request_rec *r, char const *file);
static int print_error(global_vars_t *gv, request_rec *r, enum html_error_t errorcode, int isshowcode, const char *extinfo);
static void create_fname(global_vars_t *gv, char *fname, const char *bbs, const char *key);
static void html_foot_im(global_vars_t *, request_rec *, int, int);
static void html_head(global_vars_t *gv, request_rec *r, int level, char const *title, int line);
static void html_foot(global_vars_t *gv, request_rec *r, int level, int line, int stopped);
static void kako_dirname(char *buf, const char *key);
static int getLineMax(global_vars_t *);
static int IsBusy2ch(const global_vars_t *);
#ifdef USE_INDEX
static apr_off_t getFileSize(request_rec *r, char const *file);
#endif
static int isprinted(const global_vars_t *gv, int lineNo);
#ifdef RELOADLINK
static void html_reload(global_vars_t *, request_rec *, int);
#endif

/* <ctype.h>等とかぶってたら、要置換 */
#define false (0)
#define true (!false)
#define _C_ (1<<0) /* datチェック用区切り文字等 */
#define _U_ (1<<1) /* URLに使う文字 */
#define _S_ (1<<2) /* SJIS1バイト目＝<br>タグ直前の空白が削除可かを適当に判定 */

/* #define isCheck(c) (flagtable[(unsigned char)(c)] & _C_) */
#define isCheck(c) (flagtable[/*(unsigned char)*/(c)] & _C_)
#define isSJIS1(c) (flagtable[(unsigned char)(c)] & _S_)
#define hrefStop(c) (!(flagtable[(unsigned char)(c)] & _U_))

#define	LINK_URL_MAXLEN		256
		/*	レス中でURLとみなす文字列の最大長。短い？	*/

#define _0____ (1<<0)
#define __1___ (1<<1)
#define ___2__ (1<<2)
#define ____3_ (1<<3)
#define _____4 (1<<4)

#define ______ (0)
#define _01___ (_0____|__1___|0)
#define __1_3_ (__1___|____3_|0)
#define ___23_ (___2__|____3_|0)
#define _0_23_ (_0____|___2__|____3_|0)


/*
	'\n'と':'を isCheck(_C_) に追加
	TYPE_TERIの時は、'\x81'と','をはずしてみた
	すこーーしだけ違うかも
*/
static const char flagtable[256] = {
	_0____,______,______,______,______,______,______,______, /*  00-07 */
	______,______,_0____,______,______,______,______,______, /*  08-0F */
	______,______,______,______,______,______,______,______, /*  10-17 */
	______,______,______,______,______,______,______,______, /*  18-1F */
	_0____,__1___,______,__1___,__1___,__1___,_01___,______, /*  20-27 !"#$%&' */
#if	defined(TYPE_TERI) && !defined(AUTO_LOGTYPE)
	______,______,__1___,__1___,__1___,__1___,__1___,__1___, /*  28-2F ()*+,-./ */
#else
	______,______,__1___,__1___,_01___,__1___,__1___,__1___, /*  28-2F ()*+,-./ */
#endif
	__1___,__1___,__1___,__1___,__1___,__1___,__1___,__1___, /*  30-37 01234567 */
	__1___,__1___,_01___,__1___,_0____,__1___,______,__1___, /*  38-3F 89:;<=>? */
	__1_3_,__1_3_,__1_3_,__1_3_,__1_3_,__1_3_,__1_3_,__1_3_, /*  40-47 @ABCDEFG */
	__1_3_,__1_3_,__1_3_,__1_3_,__1_3_,__1_3_,__1_3_,__1_3_, /*  48-4F HIJKLMNO */
	__1_3_,__1_3_,__1_3_,__1_3_,__1_3_,__1_3_,__1_3_,__1_3_, /*  50-57 PQRSTUVW */
	__1_3_,__1_3_,__1_3_,__1_3_,__1_3_,__1_3_,__1_3_,__1_3_, /*  58-5F XYZ[\]^_ */
	__1_3_,__1_3_,__1_3_,__1_3_,__1_3_,__1_3_,__1_3_,__1_3_, /*  60-67 `abcdefg */
	__1_3_,__1_3_,__1_3_,__1_3_,__1_3_,__1_3_,__1_3_,__1_3_, /*  68-6F hijklmno */
	__1_3_,__1_3_,__1_3_,__1_3_,__1_3_,__1_3_,__1_3_,__1_3_, /*  70-77 pqrstuvw */
	__1_3_,__1_3_,__1_3_,__1_3_,__1_3_,__1_3_,__1_3_,______, /*  78-7F xyz{|} */
#if	defined(TYPE_TERI) && !defined(AUTO_LOGTYPE)
	____3_,___23_,___23_,___23_,___23_,___23_,___23_,___23_, /*  80-87 */
#else
	____3_,_0_23_,___23_,___23_,___23_,___23_,___23_,___23_, /*  80-87 */
#endif
	___23_,___23_,___23_,___23_,___23_,___23_,___23_,___23_, /*  88-8F */
	___23_,___23_,___23_,___23_,___23_,___23_,___23_,___23_, /*  90-97 */
	___23_,___23_,___23_,___23_,___23_,___23_,___23_,___23_, /*  98-9F */
	____3_,____3_,____3_,____3_,____3_,____3_,____3_,____3_, /*  A0-A7 */
	____3_,____3_,____3_,____3_,____3_,____3_,____3_,____3_, /*  A8-AF */
	____3_,____3_,____3_,____3_,____3_,____3_,____3_,____3_, /*  B0-B7 */
	____3_,____3_,____3_,____3_,____3_,____3_,____3_,____3_, /*  B8-BF */
	____3_,____3_,____3_,____3_,____3_,____3_,____3_,____3_, /*  C0-C7 */
	____3_,____3_,____3_,____3_,____3_,____3_,____3_,____3_, /*  C8-CF */
	____3_,____3_,____3_,____3_,____3_,____3_,____3_,____3_, /*  D0-D7 */
	____3_,____3_,____3_,____3_,____3_,____3_,____3_,____3_, /*  D8-DF */
	___23_,___23_,___23_,___23_,___23_,___23_,___23_,___23_, /*  E0-E7 */
	___23_,___23_,___23_,___23_,___23_,___23_,___23_,___23_, /*  E8-EF */
	___23_,___23_,___23_,___23_,___23_,___23_,___23_,___23_, /*  F0-F7 */
	___23_,___23_,___23_,___23_,___23_,______,______,______, /*  F8-FF */
};

typedef struct { /*  class... */
	char **buffers; /* csvの要素 */
	int rest; /* 残りのバッファサイズ・・厳密には判定してないので、数バイトは余裕が欲しい */
} ressplitter;

/* (元w)global変数の初期化 */
static int init_global_vars(global_vars_t **gv, request_rec *r)
{
#ifdef USE_SETTING_FILE
	int i;
#endif
	*gv = apr_palloc(r->pool, sizeof **gv);
	if (!*gv) return 1;
	(*gv)->cwd = ap_make_dirstr_parent(r->pool, r->filename);

#ifdef USE_MMAP
	(*gv)->zz_mmap = NULL;
	(*gv)->zz_mmap_f = NULL;
#endif

#ifdef READ_KAKO
	*(*gv)->read_kako = '\0';
#endif
	(*gv)->nn_range.count = 0;

	(*gv)->BigBuffer = NULL;

	(*gv)->zz_fileSize = 0;
	(*gv)->lineMax = -1;
	(*gv)->out_resN = 0;

#if defined(SEPARATE_CHUNK_ANCHOR) || defined(PREV_NEXT_ANCHOR)
	(*gv)->first_line = 1;
	(*gv)->last_line = 1;
#endif

	(*gv)->isbusytime = 0;

#ifdef PREV_NEXT_ANCHOR
	(*gv)->need_tail_comment = 0;
#endif

#ifdef USE_SETTING_FILE
	(*gv)->Settings = defaultSettings; /* same as memcpy() */
	for (i = 0; i < arraylen(defaultSettingParam); i++) {
		(*gv)->SettingParam[i].str = defaultSettingParam[i].str;
		(*gv)->SettingParam[i].val = (int *)((char *)&(*gv)->Settings+(size_t)defaultSettingParam[i].val);
	}
#endif

	(*gv)->create_link_mid_start = NULL;
	return 0;
}

/****************************************************************/
/* 範囲リスト range へ追加する
 *
 * st, toの期待内容(aaa,bbbは実際は数字):
 *   入力文字列   st    to
 *   ""        -> "-"   "-"    無視
 *   "aaa"     -> "aaa" "-"    単独
 *   "aaa-"    -> "aaa" ""     先頭指定
 *   "-bbb"    -> "-"   "bbb"  末尾指定
 *   "-"       -> "-"   ""     全範囲
 *   "aaa-bbb" -> "aaa" "bbb"  両端指定
 *
 * aaa > bbb ならば aaa のみとして扱う。
 *
 * rangeの内容は常に昇順を維持する。
 * rangeが満杯ならば追加しない。
 */
static void add_range(struct range *range, const char *st, const char *to)
{
	int i_st;
	int i_to;
	int i;
	int hole;

	if ( *st == '-' && *to == '-' )
		return;

	i_st = atoi(st);
	if ( i_st < 1 )
		i_st = 0;

	switch ( *to ) {
	case '-':
		i_to = i_st;
		break;
	case '\0':
		i_to = RANGE_MAX_RES;
		break;
	default:
		i_to = atoi(to);
	}
	if ( i_to < i_st )
		i_to = i_st;

	/* merge */
	hole = -1;
	for ( i = range->count - 1 ; i >= 0 ; --i ) {
		if ( i_st <= (range->array[i].to + 1) && (range->array[i].from - 1) <= i_to ) {
			if ( i_st > range->array[i].from )
				i_st = range->array[i].from;
			if ( i_to < range->array[i].to )
				i_to = range->array[i].to;

			if ( hole >= 0 )
				if ( --range->count != hole )
					memcpy( &range->array[hole], &range->array[hole+1], (range->count - hole) * sizeof range->array[hole]);

			hole = i;
		}
	}

	/* append */
	if ( hole < 0 ) {
		if ( range->count >= MAX_RANGE )
			return;
		hole = range->count++;
	}

	while ( hole > 0 && i_st < range->array[hole-1].from ) {
		range->array[hole] = range->array[hole - 1];
		--hole;
	}
	range->array[hole].from = i_st;
	range->array[hole].to = i_to;
}

/* rangeの上下限取得
 * *st以降全部なら *to = 0となる
 */
static int get_range_minmax(const struct range *range, int *st, int *to)
{

	int i_st, i_to;
	if ( range->count <= 0 )
		return false;

	i_st = range->array[0].from;
	i_to = range->array[range->count-1].to;
	if ( i_to >= RANGE_MAX_RES )
		i_to = 0;
	*st = i_st;
	*to = i_to;
	return true;
}

/* range内判定
   rangeが空ならばfalseを返してしまうので注意
 */
static int in_range(const struct range *range, int lineNo)
{
	int i;
	if ( range->count == 0 
	  || range->array[0].from > lineNo || range->array[range->count-1].to < lineNo )
		return false;

	for ( i = 0 ; i < range->count ; ++i ) {
		if ( range->array[i].from <= lineNo &&
		     range->array[i].to >= lineNo )
			return true;
	}
	return false;
}

/* 数字、ハイフン、カンマだけからなる文字列からrangeを組み立てる
 */
static const char *add_ranges_simple(struct range *range, const char *p)
{
	int st, to;
	for (;;) {
		char w_st[12];
		char w_to[12];
		while ( *p == ',' )
			++p;
		if ( *p != '-' && !isdigit(*p) )
			break;
		st = to = atoi(p);
		sprintf( w_st, "%d", st );
		sprintf( w_to, "%d", to );
		while ( isdigit(*p) )
			++p;
		if (*p == '-') {
			++p;
			if ( !isdigit(*p) )
				w_to[0] = '\0';
			else {
				to = atoi(p);
				sprintf( w_to, "%d", to );
			}
			while (isdigit(*p))
				++p;
		}
		add_range( range, w_st, w_to );
	} 
	return p;
}

/****************************************************************/
/* linkの先頭部分(板まで)の生成
 * 戻り値は終端null文字を指す
 */
static char *create_link_head(const global_vars_t *gv, char *url_expr)
{
	char *p = url_expr;
#if defined(USE_INDEX) || defined(CREATE_OLD_LINK)
	const char *key = gv->zz_ky;
#ifdef READ_KAKO
	if ( gv->read_kako[0] )
		key = gv->read_kako;
#endif
#endif

#ifdef	CREATE_OLD_LINK
	if (gv->path_depth) {
#endif
#ifdef USE_INDEX
		if (gv->path_depth == 2) {
			p += sprintf(p, "%.40s/", key);
		}
#endif
#ifdef	CREATE_OLD_LINK
	} else {
		p = url_expr;
		p += sprintf(gv->url_p, "\"" CGINAME "?bbs=%.20s&key=%.40s", gv->zz_bs, key);
	}
#endif
	*p = '\0';
	return p;
}

/* linkの中間部分の生成
 */
static char *create_link_mid(const global_vars_t *gv, char *p, int st, int to, int ls)
{
#ifdef	CREATE_OLD_LINK
	if (gv->path_depth) {
#endif
		if (ls) {
			p += sprintf(p,"l%d",ls);
		} else if (to==0) {
			if (st>1)
				p += sprintf(p,"%d-",st);
			
		} else {
			if ( st != to ) {
				if ( st > 1 )
					p += sprintf(p, "%d", st); /* 始点 */
				*p++ = '-';
			}
			p += sprintf(p, "%d", to); /* 終点 */
		}
#ifdef	CREATE_OLD_LINK
	} else {
		if (ls) {
			p += sprintf(p,"&ls=%d",ls);
		} else {
			if ( st > 1 )
				p += sprintf(p, "&st=%d", st);
			if ( to )
				p += sprintf(p, "&to=%d", to);
		}
	}
#endif
	*p = '\0';
	return p;
}

/* linkの末尾部分の生成
 */
static char *create_link_tail(const global_vars_t *gv, char *url_expr, char *p, int st, int nf, int sst)
{
#ifdef	CREATE_OLD_LINK
	if (gv->path_depth) {
#endif
		if (nf)
			*p++ = 'n';
		if (is_imode())
			*p++ = 'i';
#ifdef CREATE_NAME_ANCHOR
		if (sst && sst!=st) {
			p += sprintf(p,"#%d",sst);
		}
#endif
		if ( p == url_expr )
			p += sprintf(p, "./"); /* 全部 */
#ifdef	CREATE_OLD_LINK
	} else {
		if (nf)
			p += sprintf(p, NO_FIRST);
		if (is_imode()) {
			p += sprintf(p, "&imode=true");
		}
#ifdef CREATE_NAME_ANCHOR
		if (sst && sst!=st) {
			p += sprintf(p,"#%d",sst);
		}
#endif
		*p++ = '\"';
	}
#endif
	*p = '\0';
	return p;
}

/* read.cgi呼び出しのLINK先作成 */
/* 一つの ap_rprintf() で一度しか使ってはいけない */
/* st,to,ls,nfは、それぞれの呼び先。nf=1でnofirst=true
** 使わないものは、0にして呼ぶ
** sstは、CHUNK_LINKの場合の#番号
*/
static const char *create_link(global_vars_t *gv, int st, int to, int ls, int nf, int sst)
{
	char *url_expr = gv->create_link_url_expr;
	char *mid_start = gv->create_link_mid_start;
	char *p;
	if ( !mid_start )
		mid_start = create_link_head(gv, url_expr);
	p = mid_start;
	if (is_imode() && st==0 && to==0)	/* imodeの0-0はls=10相当 */
		ls = 10;
	if ( nf && ls == 0 )
		if ( st == 1 || st == to )	/* 単点と1を含むときはは'n'不要 */
			nf = 0;
	p = create_link_mid(gv, p, st, to, ls);
	create_link_tail(gv, url_expr, p, st, nf, sst);
	return url_expr;
}

/* rangeに従ってリンク生成
 * 戻り値は文字数
 */
static int create_link_range(const global_vars_t *gv, char *url_expr, const struct range *range, int nf, int sst)
{
	int st, to;
	char *p;
	int i;
	st = to = 0;

	get_range_minmax( range, &st, &to );

	p = create_link_head(gv, url_expr);
	if (is_imode() && st==0 && to==0) {	/* imodeの0-0はls=10相当 */
		p = create_link_mid(gv, p, 0, 0, 10);
  	} else {
		if ( st <= 1 || st == to )	/* 単点と1を含むときはは'n'不要 */
			nf = 0;
		for ( i = 0 ; i < range->count ; ++i ) {
			int i_to;
#ifdef	CREATE_OLD_LINK
			if (gv->path_depth)
#endif
			{
				if ( i > 0 ) {
					*p++ = ',';
				}
			}
			i_to = range->array[i].to;
			if ( i_to >= RES_RED )
				i_to = 0;
			p = create_link_mid(gv, p, range->array[i].from, i_to, 0);
		}
	}

	p = create_link_tail(gv, url_expr, p, st, nf, sst);
	return p - url_expr;
}

/* ディレクトリを指定段上がる文字列を作成する
 *
 * 戻り値は生成文字数
 * bufのサイズは (up * 3 + 1)必要
 */
static int up_path(char *buf, size_t up)
{
	size_t i;
	char *w = buf;
	for ( i = 0 ; i < up ; ++i )
	{
		w[0] = '.';
		w[1] = '.';
		w[2] = '/';
		w += 3;
	}
	*w = '\0';
	return w - buf;
}

/* 掲示板に戻るのLINK先作成 */
static void zz_init_parent_link(global_vars_t *gv)
{
	char *p = gv->zz_parent_link;

#ifdef USE_INDEX
	if (gv->path_depth==2)
		p += up_path(p, 1);
	else
#endif
	{
		p += up_path(p, gv->path_depth+1);
		p += sprintf(p, "%.20s/", gv->zz_bs);
	}
	if (is_imode()) {
		strcpy(p,"i/");
		return;
	}
}

/* bbs.cgiのLINK先作成 */
static void zz_init_cgi_path(global_vars_t *gv)
{
	gv->zz_cgi_path[0] = '\0';
	up_path( gv->zz_cgi_path, gv->path_depth );
}

/*
  初期化
  toparray ポインタ配列のアドレス
  buff コピー先のバッファの先頭
  bufsize 厳密には判定してないので、数バイトは余裕が欲しい
  */

static void ressplitter_init(ressplitter *This, char **toparray, char *buff, int bufsize)
{
	This->buffers = toparray;
	This->rest = bufsize;
	*This->buffers = buff;
}

/* <a href="xxx">をPATH向けに打ち直す
   *spは"<a "から始まっていること。
   dp, sp はポインタを進められる
   書き換える場合は</a>まで処理するが、
   書き換え不要な場合は<a ....>までママコピ(もしくは削る)だけ
   走査不能な場合は、0 を戻す

   ad hocに、中身が&gt;で始まってたら
   href情報をぜんぶ捨てて、
   <A></A>で囲まれた部位を取り出し、
   自ら打ち直すようにする
   想定フォーマットは以下のもの。
   &gt;&gt;nnn
   &gt;&gt;xxx-yyy */
static int rewrite_href(const global_vars_t *gv,
			char **dp,		/* 書き込みポインタ */
			char const **sp,	/* 読み出しポインタ */
			int istagcut)		/* タグカットしていいか? */
{
	char *d = *dp;
	char const *s = *sp;
	char const *copy_start;
	int copy_len;
	int dangling_len = 0;
	int st, to;
	struct range range;
	int close_anchor = false;
	range.count = 0;
	
	while (*s != '>' && *s != '\n')
		s++;
	if (memcmp(s, ">http://", 8) == 0) {
		/* URLリンクだった場合、そのまま出力してみる */
		s += 8; /* skip ">http://" */
		s = strstr(s, "</a>");
		if ( !s )
			return 0;
		s += 4; /* length of "</a>" */
		copy_start = *sp;
		copy_len = s - copy_start;
		memcpy( d, copy_start, copy_len );
		d += copy_len;
		*sp = s;
		*dp = d;
		return 1;
	}
	if (memcmp(s, ">&gt;&gt;", 9) == 0)
		s += 9;
	copy_start = s;

	s = add_ranges_simple(&range, s);

	copy_len = s - copy_start;
	if (copy_len == 0 || memcmp(s, "</a>", 4) != 0)
		return 0;
	s += 4; /* skip "</a>" */

	dangling_len = add_ranges_simple(&range, s) - s;

	st = to = -1;
 	if ( range.count > 0 )
		get_range_minmax( &range, &st, &to );
	if ( st < 1 )
		st = 1;
	if ( to < 1 )
		to = gv->lineMax;

	if (0 < st && st <= gv->lineMax && 0 < to && to <= gv->lineMax &&
		!(istagcut && isprinted(gv, st) && isprinted(gv, to))) {
			close_anchor = true;
#ifdef CREATE_NAME_ANCHOR
			/* 新しい表現をブチ込む */
			if (isprinted(gv, st) && isprinted(gv, to)) {
				d += sprintf(d, "<a href=#%u>", st);
			} else
#endif
			{
				static const char *target_blank[] = {
					" " TARGET_BLANK,
					""
				};
				int nofirst = true;
#if defined(CHUNK_ANCHOR) && defined(CREATE_NAME_ANCHOR) && defined(USE_CHUNK_LINK)
				nofirst = false;
				/* chunk仕様を生かすためのkludgeは以下に。 */
				mst = (st - 1) / CHUNK_NUM;
				mto = (to - 1) / CHUNK_NUM;

				if (range.count <= 1 && mst == mto && (st != 1 || to != 1)) {
					/* chunk範囲 */
					mst = mst * CHUNK_NUM + 1;
					mto = mto * CHUNK_NUM + CHUNK_NUM;
					range.count = 1;
					range.array[0].from = mst;
					range.array[0].to = mto;
				} else 
#endif
				{
					/* chunkをまたぎそうなので、最小単位を。*/
				}
				d += sprintf(d, "<a href=");
				d += create_link_range( gv, d, &range, nofirst, st );
				d += sprintf(d, "%s>", target_blank[is_imode()]);
			}
	}

	/* "&gt;&gt;"は >> に置き換え、つづき.."</a>"を丸写し */
	*d++ = '>';
	*d++ = '>';
	memcpy( d, copy_start, copy_len );
	d += copy_len;
	if ( dangling_len > 0 ) {
		if ( *s != ',' )
			*d++ = ',';
		memcpy( d, s, dangling_len );
		d += dangling_len;
		s += dangling_len;
	}
	if ( close_anchor ) {
		memcpy( d, "</a>", 4 );
		d += 4;
	}
	*sp = s;
	*dp = d;
	return 1;
}
/*
	pは、"http://"の':'を指してるよん
	何文字さかのぼるかを返す。0ならnon-match
	４文字前(httpの場合)からスキャンしているので、
	安全を確認してから呼ぶ
*/
static int isurltop(const char *p)
{
	if (strncmp(p-4, "http://", 7) == 0)
		return 7-3;
	if (strncmp(p-3, "ftp://", 6) == 0)
		return 6-3;
	return 0;
}
/*
	pは、"http://www.2ch.net/..."の最初の'w'を指してる
	http://wwwwwwwww等もできるだけ判別
	urlでないとみなしたら、0を返す
*/
static int geturltaillen(const char *p)
{
	const char *top = p;
	int len = 0;
	while (!hrefStop(*p)) {
		if (*p == '&') {
			/* &quot;で囲まれたURLの末尾判定 */
			if (strncmp(p, "&quot;", 6) == 0)
				break;
			if (strncmp(p, "&lt;", 4) == 0)
				break;
			if (strncmp(p, "&gt;", 4) == 0)
				break;
		}
		++len;
		++p;
	}
	if (len) {
		if (memchr(top, '.', len) == NULL	/* '.'が含まれないURLは無い */
			|| *(top + len - 1) == '.')	/* '.'で終わるURLは(普通)無い */
			len = 0;
		if (len > LINK_URL_MAXLEN)	/* 長すぎたら却下 */
			len = 0;
	}
	return len;
}

/*
	urlから始まるurllen分の文字列をURLとみなしてbufpにコピー。
	合計何文字バッファに入れたかを返す。
*/
static int urlcopy(char *bufp, const char *url, int urllen)
{
	return sprintf(bufp,
		"<a href=\"%.*s\" " TARGET_BLANK ">%.*s</a>", 
		urllen, url, urllen, url);
}

/*
	resnoが、
	出力されるレス番号である場合true
	範囲外である場合false
	判定はdat_out()を基本に
*/
static int isprinted(const global_vars_t *gv, int lineNo)
{
	if (lineNo == 1) {
		if (is_nofirst())
			return false;
	} else {
		if (gv->nn_st && lineNo < gv->nn_st)
			return false;
		if (gv->nn_to && lineNo > gv->nn_to)
			return false;
		if (gv->nn_ls && (lineNo-1) < gv->lineMax - gv->nn_ls)
			return false;
		if ( gv->nn_range.count > 0 ) {
			return in_range( &gv->nn_range, lineNo );
		}
	}
	return true;
}

#define	needspace_simple(buftop, bufp)	\
	((bufp) != (buftop) && isSJIS1(*((bufp)-1)))

#ifdef	CUT_TAIL_BLANK
/* 直前の文字がシフトJIS１バイト目であるかを返す。 */
static int needspace_strict(const char *buftop, const char *bufp)
{
	const char *p = bufp-1;
	while (p >= buftop && isSJIS1(*p))
		p--;
	return (bufp - p) % 2 == 0;
}
# define	needspace(buftop, bufp)	\
	(needspace_simple((buftop), (bufp)) && needspace_strict((buftop), (bufp)))
#else
# define	needspace(buftop, bufp)	\
	needspace_simple((buftop), (bufp))
#endif	/* CUT_TAIL_BLANK */

#define	add_tailspace(buftop, bufp)	\
	(needspace(buftop, bufp) ? (void) (*bufp++ = ' ') : (void)0)

#ifdef	STRICT_ILLEGAL_CHECK
# define	add_tailspace_strict(buftop, bufp)	add_tailspace(buftop, bufp)
#else
# define	add_tailspace_strict(buftop, bufp)	((void)0)
#endif

#if	defined(AUTO_LOGTYPE)
#define		IS_TYPE_TERI	(gv->zz_log_type/* != 0 */)
#elif	defined(TYPE_TERI)
#define		IS_TYPE_TERI	1
#else
#define		IS_TYPE_TERI	0
#endif
/*
	レスを全走査するが、コピーと変換(と削除)を同時に行う
	p コピー前のレス(BigBuffer内の１レス)
	resnumber	レス本文である場合にレス番号(行番号＋１)、それ以外は0を渡す
	istagcut	<a href=...>と</a>をcutするか
	Return		次のpの先頭
	non-TYPE_TERIなdatには,"<>"は含まれないはずなので、#ifdef TYPE_TERI は略
*/
static const char *ressplitter_split(const global_vars_t *gv, ressplitter *This, const char *p, int resnumber)
{
	char *bufp = *This->buffers;
	int bufrest = This->rest;
	int istagcut = (LINKTAGCUT && gv->isbusytime && resnumber > 1) || is_imode();
	/*	ループ中、*This->Buffersはバッファの先頭を保持している	*/
	while (--bufrest > 0) {
		int ch = *p;
		if (isCheck(ch)) {
			switch (ch) {
			case ' ':
				/* 無意味な空白は１つだけにする */
				while (*(p+1) == ' ')
					p++;
				if (*(p+1) != '<')
					break;
				if (*(p+2) == '>') {
					if (bufp == *This->buffers) /* 名前欄が半角空白１つの場合に必要 */
						*bufp++ = ' ';
					p += 3;
					goto Teri_Break;
				}
				if (memcmp(p, " <br> ", 6) == 0) {
					add_tailspace(*This->buffers, bufp);
					memcpy(bufp, "<br>", 4);
					p += 6;
					bufp += 4;
					continue;
				}
				break;
			case '<': /*  醜いが */
				if (*(p+1) == '>') {
					p += 2;
					goto Teri_Break;
				}
				add_tailspace_strict(*This->buffers, bufp);
				if (resnumber && p[1] == 'a' && isspace(p[2])) {
					char *tdp = bufp;
					char const *tsp = p;
					if (!rewrite_href(gv, &tdp, &tsp, istagcut))
						break; /* 走査不能な<a >タグはそのまま出力し、続行 */
					bufrest -= tdp - bufp;
					bufp = tdp;
					p = tsp;
					continue;
				}
				break;
			case '&':
				/* add_tailspace_strict(*This->buffers, bufp); */
				if (memcmp(p+1, "amp", 3) == 0) {
					if (*(p + 4) != ';')
						p += 4 - 1;
				}
				/* &MAIL->&amp;MAILの変換は、dat直読みのかちゅ～しゃが対応しないと無理
				   もし変換するならbbs.cgi */
				break;
			case ':':
#ifndef NO_LINK_URL_BUSY
				if (resnumber)
#else
				if (resnumber && !istagcut)
					/* urlのリンクを(時間帯によって)廃止するなら */
#endif
				{
					if (*(p+1) == '/' && *(p+2) == '/' && isalnum(*(p+3))) {
						/*
							正常なdat(名前欄が少なくとも1文字)ならば、
							pの直前は最低、" ,,,"となっている(さらに投稿日の余裕がある)。
							なので、4文字("http")まではオーバーフローの危険はない
							ただし、これらは、resnumberが、正確に渡されているのが前提。
						*/
						int pdif = isurltop(p);	/*	http://の場合、4が返る	*/
						if (pdif) {
							int taillen = geturltaillen(p + 3);
							if (taillen && bufrest > taillen * 2 + 60) {
								bufp -= pdif, p -= pdif, bufrest += pdif, taillen += pdif + 3;
								add_tailspace_strict(*This->buffers, bufp);
								pdif = urlcopy(bufp, p, taillen);
								bufp += pdif, bufrest -= pdif, p += taillen;
								continue;
							}
						}
					}
				}
				break;
			case '\n':
				goto Break;
				/*	break;	*/
#if	!defined(TYPE_TERI) || defined(AUTO_LOGTYPE)
			case COMMA_SUBSTITUTE_FIRSTCHAR: /*  *"＠"(8197)  "｀"(814d) */
				if (!IS_TYPE_TERI) {
					/* ここは特に頻度が低いため、可読性、保守性のほうが重要 */
					if (memcmp(p, COMMA_SUBSTITUTE, COMMA_SUBSTITUTE_LEN) == 0) {
						ch = ',';
						p += 4 - 1;
					}
				}
				break;
			case ',':
				if (!IS_TYPE_TERI) {
					p++;
					goto Break;
				}
				break;
#endif
			case '\0':
				if (p >= gv->BigBuffer + gv->zz_fileSize)
					goto Break;
				/* 読み飛ばしのほうが、動作としては適切かも */
				ch = '*';
				break;
			default:
				break;
			}
		}
		*bufp++ = ch;
		p++;
	}
Teri_Break:
	/* 名前欄に','が入っている時にsplitをミスるので、見誤る可能性があるので、 */
	/* This->isTeri = true; */
Break:
	add_tailspace(*This->buffers, bufp);
	*bufp++ = '\0';
	This->rest -= bufp - *This->buffers;
	*++This->buffers = bufp;
	
	/* 区切り直後の空白を削除 */
	if (*p == ' ') {
		if (!IS_TYPE_TERI) {
			if (!(*(p+1) == ','))
				++p;
		} else {
			if (!(*(p+1) == '<' && *(p+2) == '>'))
				++p;
		}
	}
	return p;
}

static void splitting_copy(const global_vars_t *gv, char **s, char *bufp, const char *p, int size, int linenum)
{
	ressplitter res;
	ressplitter_init(&res, s, bufp, size);
	
	p = ressplitter_split(gv, &res, p, false); /* name */
	p = ressplitter_split(gv, &res, p, false); /* mail */
	p = ressplitter_split(gv, &res, p, false); /* date */
	p = ressplitter_split(gv, &res, p, linenum+1); /* text */
	p = ressplitter_split(gv, &res, p, false); /* title */
	if (s[1][0] == '0' && s[1][1] == '\0')
		s[1][0] = '\0';
}

#ifdef	AUTO_LOGTYPE
static void check_logtype(const global_vars_t *gv)
{
	if (gv->lineMax) {
		const char *p;
		
		gv->zz_log_type = 0;	/* non-TYPE_TERI */
		for (p = gv->BigLine[0]; p < gv->BigLine[1]; ++p) {
			if (*p == '<' && *(p+1) == '>') {
				gv->zz_log_type = 1;	/* TYPE_TERI */
				break;
			}
		}
	}
}
#else
#define	check_logtype(gv)		/*	*/
#endif

/* タイトルを取得してzz_titleにコピー
*/
static void get_title(global_vars_t *gv)
{
	char *s[20];
	char p[SIZE_BUF];
	
	if (gv->lineMax) {
		splitting_copy(gv, s, p, gv->BigLine[0], sizeof(p) - 20, 0);
		strncpy(gv->zz_title, s[4], sizeof(gv->zz_title)-1);
		gv->zz_title[sizeof(gv->zz_title)-1] = '\0';
	}
}

/* ストッパー・1000 Overの判定
*/
static int isthreadstopped(const global_vars_t *gv)
{
	char *s[20];
	char p[SIZE_BUF];
	static const char *stoppers[] = {
		STOPPER_MARKS
	};
	int i;
	
	if (gv->lineMax >= RES_RED)
		return 1;
#ifdef WRITESTOP_FILESIZE
	if (gv->zz_fileSize > MAX_FILESIZE - WRITESTOP_FILESIZE * 1024)
		return 1;
#endif
	if (gv->lineMax) {
		splitting_copy(gv, s, p, gv->BigLine[gv->lineMax-1], sizeof(p) - 20, gv->lineMax-1);
		for ( i = 0 ; i < arraylen(stoppers) ; ++i )
			if ( strstr( s[2], stoppers[i] ) )
				return 1;
		return 0;
	}
	return 1;
}


/****************************************************************/
/*	BadAccess						*/
/* 許可=0, 不許可=1を返す                                       */
/****************************************************************/
static int BadAccess(const global_vars_t *gv)
{
	static const char *agent_kick[] = {
#ifdef	Katjusha_Beta_kisei
		"Katjusha",
#endif
		"WebFetch",
		"origin",
		"Nozilla"
	};
	int i;

	if ( is_head() )
		return 0;
#ifdef RAWOUT
	if ( gv->rawmode )
		return 0;
#endif
	if (!*gv->zz_http_user_agent && !*gv->zz_http_language)
		return 1;

	for (i = 0; i < arraylen(agent_kick); i++) {
		if (strstr(gv->zz_http_user_agent, agent_kick[i]))
			return 1;
	}

	return 0;
}
/****************************************************************/
/*	Log Out							*/
/****************************************************************/
static int logOut(global_vars_t *gv, request_rec *r, char const *txt)
{
#ifdef LOGLOGOUT
	char *fname;
	apr_file_t *fp;
#endif

#ifdef DEBUG
	return 0;
#endif

	if (!BadAccess(gv))
		return 0;

#ifdef	LOGLOGOUT
/*
if(strcmp(gv->zz_bs,"ascii"))	return 0;
*/
	fname = apr_pstrcat(r->pool, gv->cwd, "logout.txt", NULL);
	if (apr_file_open(&fp, fname, APR_WRITE|APR_CREATE|APR_APPEND|APR_BUFFERED|APR_XTHREAD, APR_OS_DEFAULT, r->pool))
		return 0;
	apr_file_printf(fp, "%02d:%02d:%02d %8s:", gv->tm_now.tm_hour, gv->tm_now.tm_min,
			gv->tm_now.tm_sec, gv->zz_bs);
	apr_file_printf(fp, "(%15s)", gv->zz_remote_addr);
	apr_file_printf(fp, "%s***%s\n", gv->zz_http_language, gv->zz_http_user_agent);

/*
	apr_file_printf(fp, "%s\n", gv->zz_query_string);
	if (strstr(gv->zz_http_user_agent, "compatible"))
		apr_file_printf(fp, "%s\n", gv->zz_http_language);
	else
		apr_file_printf(fp, "%s***%s\n", gv->zz_http_language, gv->zz_http_user_agent);
	if(*gv->zz_http_referer) apr_file_printf(fp, "%s\n", gv->zz_http_referer);
	else			 apr_file_printf(fp, "%s***%s\n", gv->zz_http_language, gv->zz_http_user_agent);
	else			 apr_file_printf(fp, "%s\n", gv->zz_http_user_agent);
*/

	apr_file_close(fp);
#endif
	html_error(gv, r, ERROR_LOGOUT);
	return 1;
}
/****************************************************************/
/*	HTML BANNER						*/
/****************************************************************/
static void html_bannerNew(request_rec *r)
{
	ap_rputs(R2CH_HTML_NEW_BANNER, r);
}

/****************************************************************/
/*	Get file size(out_html1)				*/
/****************************************************************/
static int out_html1(global_vars_t *gv, request_rec *r, int level)
{
	if (gv->out_resN)
		return 0;
	html_head(gv, r, level, gv->zz_title, gv->lineMax);
	gv->out_resN++;
	return 0;
}
/****************************************************************/
/*	Get file size(out_html)					*/
/****************************************************************/
static int out_html(global_vars_t *gv, request_rec *r, int level, int line, int lineNo)
{
	char *s[20];
	char *r0, *r1, *r3;
	char p[SIZE_BUF];

#ifdef	CREATE_NAME_ANCHOR
#define	LineNo_			lineNo, lineNo
#else
#define	LineNo_			lineNo
#endif

/*printf("line=%d[%s]<P>\n",line,BigLine[line]);return 0;*/

	if (!gv->out_resN) {	/* Can I write header ?   */
		html_head(gv, r, level, gv->zz_title, gv->lineMax);
	}
	gv->out_resN++;

	splitting_copy(gv, s, p, gv->BigLine[line], sizeof(p) - 20, line);
	if (!*p)
		return 1; 
	
	r0 = s[0];
	r1 = s[1];
	r3 = s[3];

	if (!is_imode()) {	/* no imode */
		if (*r3 && s[4]-r3 < 8192) {
			if (*r1) {
#ifdef SAGE_IS_PLAIN
				if (strcmp(r1, "sage") == 0)
					ap_rprintf(r,
						   R2CH_HTML_RES_SAGE("%d", "%d", "%s", "%s", "%s"),
						   LineNo_, r0, s[2], r3);
				else
#endif
					ap_rprintf(r,
						   R2CH_HTML_RES_MAIL("%d", "%d", "%s", "%s", "%s", "%s"),
						   LineNo_, r1, r0, s[2], r3);
			} else {
				ap_rprintf(r,
					   R2CH_HTML_RES_NOMAIL("%d", "%d", "%s", "%s", "%s"),
					   LineNo_, r0, s[2], r3);
			}
		} else {
			ap_rprintf(r, R2CH_HTML_RES_BROKEN_HERE("%d"),
				   lineNo);
		}
		if (gv->isbusytime && gv->out_resN > RES_NORMAL) {
#ifdef PREV_NEXT_ANCHOR
			gv->need_tail_comment = 1;
#else
#ifdef SEPARATE_CHUNK_ANCHOR
			ap_rprintf(r, R2CH_HTML_TAIL_SIMPLE("%02d:00","%02d:00"),
				   LIMIT_PM - 12, LIMIT_AM);
#else
			ap_rprintf(r, R2CH_HTML_TAIL("%s","%d"),
				   create_link(gv, lineNo,
					       lineNo + RES_NORMAL, 0,1,0),
				   RES_NORMAL);
			ap_rprintf(r,
				   R2CH_HTML_TAIL2("%s", "%d") R2CH_HTML_TAIL_SIMPLE("%02d:00", "%02d:00"),
				   create_link(gv, 0,0, RES_NORMAL, 1,0),
				   RES_NORMAL,
				   LIMIT_PM - 12, LIMIT_AM);
#endif
#endif
			return 1;
		}
	} else {		/* imode  */
		if (*r3) {
			if (*s[1]) {
				ap_rprintf(r, R2CH_HTML_IMODE_RES_MAIL,
					   lineNo, r1, r0, s[2], r3);
			} else {
				ap_rprintf(r,
					   R2CH_HTML_IMODE_RES_NOMAIL,
					   lineNo, r0, s[2], r3);
			}
		} else {
			ap_rprintf(r, R2CH_HTML_IMODE_RES_BROKEN_HERE,
				   lineNo);
		}
		if (gv->out_resN > RES_IMODE && lineNo != gv->lineMax) {
#ifndef PREV_NEXT_ANCHOR
			ap_rprintf(r, R2CH_HTML_IMODE_TAIL("%s", "%d"),
				   create_link(gv, lineNo, 
					       lineNo + RES_IMODE, 0,0,0),
				   RES_IMODE);
			ap_rprintf(r, R2CH_HTML_IMODE_TAIL2("%s", "%d"),
				   create_link(gv, 0, 0, 0 /*RES_IMODE*/, 1,0),	/* imodeはスレでls=10扱い */
				   RES_IMODE);
#endif
			return 1;
		}
	}

	return 0;
#undef	LineNo_
}
/****************************************************************/
/*	Output raw data file					*/
/****************************************************************/
#ifdef RAWOUT
static int dat_out_raw(global_vars_t *gv, request_rec *r)
{
	const char *begin = gv->BigLine[0];
	const char *end = gv->BigLine[gv->lineMax];
	char statusline[512];
	char *vp = statusline;
#ifdef	RAWOUT_PARTIAL
	int first = 0, last = 0;
#endif

#ifdef	Katjusha_DLL_REPLY
	if (gv->zz_katjusha_raw) {
		if (gv->BigBuffer[gv->raw_lastsize-1] != '\n') {
			html_error(gv, r, ERROR_ABORNED);
			return 1;
		}
		begin = gv->BigBuffer + gv->raw_lastsize;
		vp += sprintf(vp, "+OK");
	} else
#endif
#ifdef	RAWOUT_PARTIAL
	if (gv->raw_lastnum == 0 && gv->raw_lastsize == 0
		&& (gv->nn_st || gv->nn_to || gv->nn_ls > 1)) {
		/* nn_xxはnofirstの関係等で変化しているかもしれないので再算出 */
		int st = atoi(gv->zz_st), to = atoi(gv->zz_to), ls = atoi(gv->zz_ls);
		
		first = 1, last = gv->lineMax;
		if (ls > 1)		/* for Ver5.22 bug */
			st = gv->lineMax - ls + 1;
		if (0 < st && st <= gv->lineMax)
			first = st;
		if (0 < to && to <= gv->lineMax)
			last = to;
		if (first > last)
			last = first;
		
		begin = gv->BigLine[first-1];
		end = gv->BigLine[last];
		vp += sprintf(vp, "+PARTIAL");
	} else
#endif
	/* もし報告された最終レス番号およびサイズが一致していなけ
	   れば、最初の行にその旨を示す */
	/* raw_lastsize > 0 にするとnnn.0であぼーん検出を無効にできるが
	   サーバーで削除したものはクライアントでも削除されるべきである */
	if(gv->raw_lastnum > 0
		&& gv->raw_lastsize >= 0
		&& !(gv->raw_lastnum <= gv->lineMax
		 && gv->BigLine[gv->raw_lastnum] - gv->BigBuffer == gv->raw_lastsize)) {
		vp += sprintf(vp, "-INCR");
		/* 全部を送信するように変更 */
	} else {
		vp += sprintf(vp, "+OK");
		/* 差分送信用に先頭を設定 */
		begin = gv->BigLine[gv->raw_lastnum];
	}
	vp += sprintf(vp, " %d/%dK", end - begin, MAX_FILESIZE / 1024);
#ifdef	RAWOUT_PARTIAL
	if (first && last) {
		vp += sprintf(vp, "\t""Range:%u-%u/%u",
			begin - gv->BigLine[0], end - gv->BigLine[0] - 1, gv->BigLine[gv->lineMax] - gv->BigLine[0]);
		vp += sprintf(vp, "\t""Res:%u-%u/%u", first, last, gv->lineMax);
	}
#endif
#ifdef	AUTO_KAKO
	{
		static const char *messages[] = {
			"",
			"\t""Status:Stopped",
			"\t""Location:temp/",
			"\t""Location:kako/"
			/* 正確な位置を知らせる必要はないはず */
		};
		int where = gv->zz_dat_where;
		if (where || isthreadstopped())
			where++;
		vp += sprintf(vp, "%s", messages[where]);
	}
#endif
	ap_rprintf(r, "%s\n", statusline);
	/* raw_lastnum から全部を送信する */
	ap_rwrite(begin, end - begin, r);

	return 0;
}

#ifdef	RAWOUT_MULTI
static int split_multireq(const char *req, char *bbs, char *key)
{
	const char *p;
	p = strchr(req, '/');
	if (p) {
		memcpy(bbs, req, p-req);
		bbs[p-req] = '\0';
		req = p + 1;
	}
	*key = '\0';
	p = strchr(req, '.');
	if (!p)
		return 0;
	memcpy(key, req, p-req);
	key[p-req] = '\0';
	return atoi(p+1);
}

/* エラー処理で楽をするため */
struct multiout_resource {
	char *Buffer;
	apr_file_t *f;
#ifdef USE_MMAP
	apr_mmap_t *m;
#endif
	apr_size_t fileSize;
	char *info;
};

/* 戻り値は
   0   正常終了
   -1  未更新or取得済みの範囲が0以下  ("+OK 0/512K"を返す)
   正  html_error_tのエラーコード +1  ("-ERR ..."を返す) */
static int exec_multiout(global_vars_t *gv, request_rec *r, struct multiout_resource *rp, const char *req)
{
	int lastsize;
	char fname[1024];
	apr_time_t lastmod;
	char *ip = rp->info;

	lastsize = split_multireq(req, gv->zz_bs, gv->zz_ky);
	ip += sprintf(ip, "\t""Request:%s/%s.%u", gv->zz_bs, gv->zz_ky, lastsize);
	if (lastsize <= 0)
		return -1;

	create_fname(gv, fname, gv->zz_bs, gv->zz_ky);
	lastmod = getFileLastmod(gv, r, fname);
	rp->fileSize = gv->zz_fileSize;
	if (lastmod == -1 || rp->fileSize == 0)
		return 1 + ERROR_NOT_FOUND;
	ip += sprintf(ip, "\t""LastModify:%u", (unsigned)lastmod);

	if (rp->fileSize > MAX_FILESIZE)
		return 1 + ERROR_TOO_HUGE;
	if (rp->fileSize == lastsize)
		return -1;
	if (rp->fileSize < lastsize)
		return 1 + ERROR_ABORNED;

	if (apr_file_open(&rp->f, fname, APR_READ, APR_OS_DEFAULT, r->pool))
		return 1 + ERROR_NOT_FOUND;

#ifdef USE_MMAP
	if (apr_mmap_create(&rp->m, rp->f, 0, rp->fileSize, APR_MMAP_READ, r->pool))
		return 1 + ERROR_NO_MEMORY;
	rp->Buffer = rp->m->mm;
#else
	rp->Buffer = apr_palloc(r->pool, rp->fileSize);
	if (!rp->Buffer)
		return 1 + ERROR_NO_MEMORY;
	/* 全部読むのは無駄だが、基本的にmmapが使われるはずなので気にしない */
	apr_file_read(rp->fd, rp->Buffer, &rp->fileSize);
#endif
	if (rp->Buffer[lastsize-1] != '\n')
		return 1 + ERROR_ABORNED;

	ap_rprintf(r, "+OK %d/%dK%s\n",
		   rp->fileSize - lastsize, MAX_FILESIZE / 1024, rp->info);

	ap_rwrite(rp->Buffer + lastsize, rp->fileSize - lastsize, r);

	/* 資源の解放は戻ってから */
	return 0;
}

/* 処理した個数を返す
*/
static int rawout_multi(global_vars_t *gv, request_rec *r)
{
	char buff[256];
	char info[512];
	struct multiout_resource res;
	int count = 0;
	const char *query = gv->zz_query_string;

	buff[0] = '\0';
	while ((query = GetString(query, buff, sizeof(buff), "dat")) != NULL) {
		int retcode;
		buff[40] = '\0';
		/* 手抜き用の初期化 */
		res.Buffer = NULL;
		res.f = NULL;
		res.m = NULL;
		res.fileSize = 0;
		res.info = info;

		retcode = exec_multiout(gv, r, &res, buff);
		if (retcode) {
			/* エラー時の処理 */
			if (retcode < 0) {
				ap_rprintf(r, "+OK 0/%dK" "%s" "\n", MAX_FILESIZE / 1024, info);
			} else {
				print_error(gv, r, (enum html_error_t)(retcode-1), true, info);
			}
		}

		/* 手抜きの資源解放 */
#ifdef	USE_MMAP
		if (res.m)
			apr_mmap_delete(res.m);
#endif
		if (res.f)
			apr_file_close(res.f);

		if (++count > RAWOUT_MULTI_MAX)
			break;
	}
	return count;
}
#endif

#endif

/****************************************************************/
/*	Get file size(dat_out)					*/
/*	Level=0のときは、外側の出力				*/
/*	Level=1のときは、内側の出力				*/
/****************************************************************/

int dat_out(global_vars_t *gv, request_rec *r, int level)
{ 
	int line; 
	int threadStopped=0; 

#ifdef READ_KAKO
	if (gv->read_kako[0]) {
		threadStopped = 1;
		/* 過去ログはFORMもRELOADLINKも非表示にするため */
	}
#endif
	for (line = 0; line < gv->lineMax; line++) { 
		int lineNo = line + 1; 
		if (!isprinted(gv, lineNo)) 
			continue; 
		if (out_html(gv, r, level, line, lineNo)) { 
			line++; 
			break; /* 非0が返るのは、エラー時とRES_NORMAL/RES_IMODEに達した時 */ 
		} 
		if (lineNo==1 && is_imode() && gv->nn_st<=1 && gv->nn_ls==0) 
			++gv->out_resN; 
	} 
	out_html1(gv, r, level); /* レスが１つも表示されていない時にヘッダを表示する */ 

	if (isthreadstopped(gv))
		threadStopped=1;
	if ( !is_imode() )
		ap_rputs(R2CH_HTML_PREFOOTER, r);
#ifdef	AUTO_KAKO
	if (zz_dat_where) {
		ap_rputs(R2CH_HTML_CAUTION_KAKO, r);
		threadStopped = 1;
	}
#endif
#ifdef RELOADLINK
	if (
#ifdef USE_INDEX
	    !level && 
#endif
	    gv->lineMax == line && !threadStopped) {
	
		html_reload(gv, r, line);	/*  Button: Reload */
	}
#endif
	html_foot(gv, r, level, gv->lineMax, threadStopped);

	return 0;
}
/****************************************************************/
/*	Get file size(dat_read)					*/
/*	BigBuffer, BigLine, LineMaxが更新されるはず		*/
/****************************************************************/
int dat_read(global_vars_t *gv, request_rec *r,
	     char const *fname,
	     int st,
	     int to,
	     int ls)
{
	apr_file_t *in;

#ifdef	PUT_ETAG
	if (gv->BigBuffer)
		return 0;
#endif
#ifdef USE_INDEX	/* USE_DIGEST */
	gv->zz_fileSize = getFileSize(r, fname);
#endif

	if (gv->zz_fileSize > MAX_FILESIZE) {
		html_error(gv, r, ERROR_TOO_HUGE);
		return 1;
	}
	if (gv->zz_fileSize < 10) {
		html_error(gv, r, ERROR_NOT_FOUND); /* エラー種別は別にした方がいいかも */
		return 1;
	}
	if (*gv->zz_ky == '.') {
		html_error(gv, r, ERROR_NOT_FOUND);
		return 1;
	}

	gv->nn_st = st;
	gv->nn_to = to;
	gv->nn_ls = ls;

	if (apr_file_open(&in, fname, APR_READ, APR_OS_DEFAULT, r->pool)) {
		html_error(gv, r, ERROR_NOT_FOUND);
		return 0;
	}
#ifdef USE_MMAP
	if (apr_mmap_create(&gv->zz_mmap, gv->zz_mmap_f = in, 0, gv->zz_fileSize+1, APR_MMAP_READ, r->pool)) {
		html_error(gv, r, ERROR_NO_MEMORY);
		return 1;
	}
	gv->BigBuffer = gv->zz_mmap->mm;
#else
	gv->BigBuffer = apr_palloc(r->pool, gv->zz_fileSize + 32);
	if (!gv->BigBuffer) {
		html_error(gv, r, ERROR_NO_MEMORY);
		return 1;
	}

	apr_file_read(in, gv->BigBuffer, &gv->zz_fileSize);
	apr_file_close(in);
#endif

#if	defined(RAWOUT) && defined(Katjusha_DLL_REPLY)
	if (gv->zz_katjusha_raw) {
		gv->BigLine[gv->lineMax = 0] = gv->BigBuffer + gv->zz_fileSize;
		return 0;
	}
#endif
	gv->lineMax = getLineMax(gv);
#ifdef	RAWOUT
	if (gv->rawmode)
		return 0;
#endif
	check_logtype();
	get_title(gv);
/*
html_error(gv, r, ERROR_MAINTENANCE);
return 1;
*/
	return 0;
}
/****************************************************************/
/*	Get line number						*/
/****************************************************************/
static int getLineMax(global_vars_t *gv)
{
	int line = 0;
	const char *p = gv->BigBuffer;
	const char *p1;

	if (!p)
		return -8;

	p1 = gv->BigBuffer + gv->zz_fileSize;	/* p1 = 最後の\nの次のポインタ */
	while (p < p1 && *(p1-1) != '\n')	/* 最後の行末を探す 正常なdatなら問題無し */
		p1--;
	if (p1 - p < 10)	/* 適当だけど、問題は出ないはず */
		return -8;
	do {
		if ( line >= arraylen(gv->BigLine) - 1 )
		{
			/* 上限行数を超えるようなら、ここを以ってファイル末尾とみなす
			   こうすることによって溢れた分すべてを巨大な1行として処理して
			   しまわないようにする
			*/
			gv->zz_fileSize = p - gv->BigBuffer;
			break;
		}
		gv->BigLine[line] = p;
		++line;
		p = (char *)memchr(p, '\n', p1-p);
		if (p == NULL)
			break;
		++p;
	} while (p < p1);
	
	/*
		最後のレスの次に、ファイル末へのポインタを入れておく。
		これにより、レスの長さはポインタの差ですむ。
		(dat_out_rawでstrlenしている部分への対応)
	*/
	gv->BigLine[line] = gv->BigBuffer + gv->zz_fileSize;
	return line;
}
/****************************************************************/
/*	Get file size						*/
/****************************************************************/
#ifdef USE_INDEX
static apr_off_t getFileSize(request_rec *r, char const *file)
{
	apr_finfo_t CountStat;
	apr_off_t ccc = 0;
	if (!apr_stat(&CountStat, file, APR_FINFO_MIN, r->pool))
		ccc = CountStat.size;
	return ccc;
}
#endif
/****************************************************************/
/*	Get file last-modified(getFileLastmod)			*/
/****************************************************************/
static apr_time_t getFileLastmod(global_vars_t *gv, request_rec *r, char const *file)
{
	apr_finfo_t CountStat;
	if (!apr_stat(&CountStat, file, APR_FINFO_MIN, r->pool)) {
		gv->zz_fileSize = CountStat.size;
		return CountStat.mtime;
	} else
		return -1;
}
/****************************************************************/
/*	Get file last-modified(get_lastmod_str)			*/
/****************************************************************/
#define get_lastmod_str(buf, lastmod) apr_rfc822_date(buf, lastmod)

/****************************************************************/
/*	PATH_INFOを解析						*/
/*	/board/							*/
/*	/board/							*/
/*	/board/datnnnnnn/[range] であるとみなす			*/
/*	return: pathが有効だったら1を返す			*/
/*	副作用: zz_bs, zz_ky, zz_st, zz_to, zz_nf		*/
/*		などが更新される場合がある			*/
/****************************************************************/
static void parse_path_param(global_vars_t *gv, const char *s)
{
	/* st/to 存在ほかのチェックのため "-"を入れておく */
	strcpy(gv->zz_st, "-");
	strcpy(gv->zz_to, "-");

	/* strtok()で切り出したバッファは総長制限が
	   かかっているので、buffer overrunはないはず */
	while (s[0]) {
		char *p;
		/* 範囲指定のフォーマットは以下のものがある

		   /4	(st=4&to=4)
		   /4-	(st=4)
		   /-6	(to=6)
		   /4-6	(st=4to=4)
		   /l10 (ls=10)
		   /i   (imode=true)
		   /.   (nofirst=false)
		   /n   (nofirst=true)

		   カキコ1は特別扱いで、nofirstにより
		   動作が左右されるっぽいので、どうしよう */

		/* st を取り出す */
		if (isdigit(*s)) {
			for (p = gv->zz_st; isdigit(*s); p++, s++)
				*p = *s;
			*p = 0;
		} else if (*s == 'i') {
			set_imode_true();
			s++;
		} else if (*s == '.') {
			set_nofirst_false();
			s++;
		} else if (*s == 'n') {
			set_nofirst_true();
			s++;
		} else if (*s == 'l') {
			s++;
			/* lsを取り出す */
			if (isdigit(*s)) {
				for (p = gv->zz_ls; isdigit(*s); p++, s++)
					*p = *s;
				*p = 0;
			}
#if 0
			/* ls= はnofirst=true を標準に */
			if (!gv->zz_nf[0]) {
				set_nofirst_true();
			}
#endif
		} else if (*s == '-') {
			s++;
			/* toを取り出す */
			if (isdigit(*s)) {
				for (p = gv->zz_to; isdigit(*s); p++, s++)
					*p = *s;
				*p = 0;
			} else {
				/* to=∞とする */
				gv->zz_to[0] = '\0';
			}
		} else if (*s == ',') {
			s++;
			add_range( &gv->nn_range, gv->zz_st, gv->zz_to );
			*gv->zz_st = *gv->zz_to = '-';
		} else {
			/* 規定されてない文字が来たので評価をやめる */
			if (strchr(s, '/'))
				gv->need_basehref = 1;
			break;
		}
	}

	add_range( &gv->nn_range, gv->zz_st, gv->zz_to );
	*gv->zz_st = *gv->zz_to = '\0';

	if ( gv->zz_ls[0] == '\0' && gv->nn_range.count > 0 ) {
		get_range_minmax( &gv->nn_range, &gv->nn_st, &gv->nn_to );

		sprintf( gv->zz_st, "%d", gv->nn_st );
		if ( gv->nn_st > 0 && gv->nn_st == gv->nn_to ) {
			/* 単点は、nofirst=trueをdefaultに */
			if (!gv->zz_nf[0]) {
				set_nofirst_true();
			}
		} else {
			sprintf( gv->zz_to, "%d", gv->nn_to );
		}
	}
}

static const char *get_path_token(global_vars_t *gv, const char *s, char *buf)
{
	const char *next = strchr(s, '/');
	if (next) {
		memcpy(buf, s, next-s);
		buf[next-s] = '\0';
		gv->path_depth++;
		s = next + 1;
	}
	return s;
}

static int get_path_info(global_vars_t *gv, char const *path_info)
{
	char const *s = path_info;

#if	1
	/* index.html がbaseを出力した場合に一応備える。邪魔なら消して。 */
	static const char index_based_path[] = "/test/" CGINAME;
	if (memcmp(s, index_based_path, sizeof(index_based_path)-1) == 0) {
		s += sizeof(index_based_path)-1;
		gv->need_basehref = 1;
	}
#endif
	gv->path_depth = 0;
	/* PATH_INFO は、'/' で始まってるような気がしたり */
	if (s[0] != '/')
		return 0;
	
	/* 長すぎるPATH_INFOは怖いので受け付けない */
	if (strlen(s) >= 256)
		return 0;
	
	++s, ++gv->path_depth;
	s = get_path_token(gv, s, gv->zz_bs);
#ifdef	READ_KAKO
	if (memcmp(s, "kako/", 5) == 0
#ifdef	READ_TEMP
		|| memcmp(s, "temp/", 5) == 0
#endif
		) {
		memcpy(gv->read_kako, s, 5);
		get_path_token(gv, s += 5, gv->read_kako+5);
	}
#endif
	s = get_path_token(gv, s, gv->zz_ky);
	
	if (!*gv->zz_ky && isdigit(*s)/* && strlen(s) >= 9*/) {
		/* /test/read.cgi/board/999999999 でもリンクするための処置 */
		strcpy(gv->zz_ky, s);
#ifdef	READ_KAKO
		if (gv->read_kako[0]) {
			strcpy(gv->read_kako+5, s);
			++gv->path_depth;
		}
#endif
		s += strlen(s);
		++gv->path_depth;
		gv->need_basehref = 1;
	}

	parse_path_param(gv, s);
	/* 処理は完了したものとみなす */
	return 1;
}

/****************************************************************/
/*	SETTING_R.TXTの読みこみ					*/
/****************************************************************/
#ifdef	USE_SETTING_FILE
static void parseSetting(global_vars_t *gv, const void *mmptr, int size)
{
	/* SETTING_R.TXTを読む */
	char const *cptr;
	char const *endp;
	int i;
	for (i = 0; i < arraylen(gv->SettingParam); i++)
		gv->SettingParam[i].len = strlen(gv->SettingParam[i].str);
	for (cptr = mmptr, endp = cptr + size - 1;
	     cptr < endp && *endp != '\n'; endp--)
		;
	for ( ; cptr && cptr < endp;
	      cptr = memchr(cptr, '\n', endp - cptr), cptr?cptr++:0) {
		if (*cptr != '\n' && *cptr != '#' && *cptr != ';') {
			int i;
			for (i = 0; i < arraylen(gv->SettingParam); i++) {
				int len = gv->SettingParam[i].len;
				if (cptr + len < endp 
				    && cptr[len] == '='
				    && strncmp(cptr,
					       gv->SettingParam[i].str,
					       len) == 0) {
					*gv->SettingParam[i].val
						= atoi(cptr + len + 1);
					break;
				}
			}
		}
	}
}

static void readSettingFile(global_vars_t *gv, request_rec *r, const char *bbsname)
{
#ifndef USE_INTERNAL_SETTINGS
	char fname[1024];
	apr_file_t *f;
	sprintf(fname, "%s../%.256s/%s", gv->cwd, bbsname, SETTING_FILE_NAME);
	if (!apr_file_open(&f, fname, APR_READ, APR_OS_DEFAULT, r->pool)) {
#ifdef	USE_MMAP
		apr_finfo_t finfo;
		apr_mmap_t *m;
		if (!apr_file_info_get(&finfo, APR_FINFO_MIN, f)
		    && !apr_mmap_create(&m, f, 0, finfo.size, APR_MMAP_READ, r->pool)) {
			parseSetting(gv, m->mm, finfo.size);
#ifdef  EXPLICIT_RELEASE
			apr_mmap_delete(m);
			apr_file_close(f);
#endif
		}
#else
		/* まあ設定ファイルが8k以上逝かなければいいということで */
		char mmbuf[8192];
		int size = sizeof mmbuf;
		apr_file_read(f, mmbuf, &size);
		parseSetting(gv, mmbuf, size);
		apr_file_close(f);
#endif	/* USE_MMAP */
	}
#else	/* USE_INTERNAL_SETTINGS */
	static const struct _setting {
		char *board_name;
		char *settings;
	} special_setting[] = {
		SPECIAL_SETTING
		{ NULL }
	};
	const struct _setting *setting;
	for (setting = special_setting; setting->board_name; setting++) {
		if (!strcmp(bbsname, setting->board_name)) {
			parseSetting(gv, setting->settings, strlen(setting->settings));
			break;
		}
	}
#endif	/* USE_INTERNAL_SETTINGS */
}
#endif	/*	USE_SETTING_FILE	*/

/****************************************************************/
/*	cell-phone detection					*/ 
/****************************************************************/
static int is_imode_agent(const char *user_agent)
{
	static const char *small_screen_agents[] = {
	"DoCoMo/",	/* i-Mode */
	"UP.Browser/",	/* EZWeb */
	"J-PHONE/", 	/* J-Phone */
	"ASTEL/"	/* Dot i */
	};
	int i;
	int len;
	int user_agent_len;

	user_agent_len = strlen( user_agent );

	for ( i = 0 ; i < arraylen(small_screen_agents) ; ++i )
	{
		len = strlen( small_screen_agents[i] );
		if ( len < user_agent_len && !memcmp(user_agent, small_screen_agents[i], len) )
		{
			return true;
		}
	}
	return false;
}
/****************************************************************/
/*	GET Env							*/
/****************************************************************/
static void zz_GetEnv(global_vars_t *gv, request_rec *r)
{
	gv->currentTime = (gv->t_now = apr_time_now()) / APR_USEC_PER_SEC;
	apr_time_exp_lt(&gv->tm_now, gv->t_now);

	gv->zz_head_request = r->header_only;
	gv->zz_remote_addr = r->connection->remote_ip;
	gv->zz_remote_host = ap_get_remote_host(r->connection, NULL, REMOTE_DOUBLE_REV, NULL);
	gv->zz_http_referer = apr_table_get(r->subprocess_env, "HTTP_REFERER");
	gv->zz_server_name = r->hostname;
	if (!gv->zz_server_name)
		gv->zz_server_name = r->server->server_hostname;
	gv->zz_script_name = apr_table_get(r->subprocess_env, "SCRIPT_NAME");
	gv->zz_path_info = r->path_info;
	gv->zz_query_string = r->args;
	gv->zz_temp = r->user;
	gv->zz_http_user_agent = apr_table_get(r->subprocess_env, "HTTP_USER_AGENT");
	gv->zz_http_language = apr_table_get(r->subprocess_env, "HTTP_ACCEPT_LANGUAGE");
	gv->zz_http_if_modified_since = apr_table_get(r->subprocess_env, "HTTP_IF_MODIFIED_SINCE");

	if (!gv->zz_remote_addr)
		gv->zz_remote_addr = KARA;
	if (!gv->zz_remote_host)
		gv->zz_remote_host = KARA;
	if (!gv->zz_http_referer)
		gv->zz_http_referer = KARA;
	if (!gv->zz_path_info)
		gv->zz_path_info = "";	/* XXX KARAを使い回すのは怖い */
	if (!gv->zz_query_string)
		gv->zz_query_string = KARA;
	if (!gv->zz_temp)
		gv->zz_temp = KARA;
	if (!gv->zz_http_user_agent)
		gv->zz_http_user_agent = KARA;
	if (!gv->zz_http_language)
		gv->zz_http_language = KARA;

	gv->zz_bs[0] = gv->zz_ky[0] = gv->zz_ls[0] = gv->zz_st[0] = '\0';
	gv->zz_to[0] = gv->zz_nf[0] = gv->zz_im[0] = '\0';
	if (!get_path_info(gv, gv->zz_path_info)) {
		/* これ以降、path が付与されているかどうかの
		   判定は zz_path_info のテストで行ってくれ */
		gv->zz_path_info = NULL;
	}
	GetString(gv->zz_query_string, gv->zz_bs, sizeof(gv->zz_bs), "bbs");
	GetString(gv->zz_query_string, gv->zz_ky, sizeof(gv->zz_ky), "key");
	GetString(gv->zz_query_string, gv->zz_ls, sizeof(gv->zz_ls), "ls");
	GetString(gv->zz_query_string, gv->zz_st, sizeof(gv->zz_st), "st");
	GetString(gv->zz_query_string, gv->zz_to, sizeof(gv->zz_to), "to");
	GetString(gv->zz_query_string, gv->zz_nf, sizeof(gv->zz_nf), "nofirst");
	if ( is_imode_agent( gv->zz_http_user_agent ) )
		set_imode_true();
	GetString(gv->zz_query_string, gv->zz_im, sizeof(gv->zz_im), "imode");
#ifdef RAWOUT
	gv->zz_rw[0] = '\0';
	GetString(gv->zz_query_string, gv->zz_rw, sizeof(gv->zz_rw), "raw");
#endif

#ifdef READ_KAKO
	if (strncmp(gv->zz_ky, "kako/", 5)==0) {
		strcpy(gv->read_kako, gv->zz_ky);
		strcpy(gv->zz_ky, gv->zz_ky+5);
	} else if (strncmp(gv->zz_ky, "temp/", 5)==0) {
		strcpy(gv->read_kako, gv->zz_ky);
		strcpy(gv->zz_ky, gv->zz_ky+5);
	}
#endif
	/* zz_ky は単なる32ビット数値なので、
	   以降、数字でも扱えるようにしておく */
	gv->nn_ky = atoi(gv->zz_ky);

#ifdef RAWOUT
	gv->rawmode = (*gv->zz_rw != '\0');
	if (gv->rawmode) {
		char *p = strchr(gv->zz_rw, '.');
		if(p) {
			/* raw=(last_article_no).(local_dat_size) */
			gv->raw_lastsize = atoi(p + 1);
		}
		gv->raw_lastnum = atoi(gv->zz_rw);
		if (gv->raw_lastnum < 0)
			gv->raw_lastnum = 0;
		if (!p || gv->raw_lastsize < 0)
			gv->raw_lastsize = 0;	/* -INCR を返すため */
#ifdef	Katjusha_DLL_REPLY
		gv->zz_katjusha_raw = gv->zz_rw[0] == '.' && gv->raw_lastsize > 0
				      /*&& gv->strstr(gv->zz_http_user_agent, "Katjusha")*/;
#endif
	}
#endif
#ifdef	USE_SETTING_FILE
	readSettingFile(gv, r, gv->zz_bs);
#endif
	gv->isbusytime = IsBusy2ch(gv);
}

/*----------------------------------------------------------------------
	終了処理
----------------------------------------------------------------------*/
static void atexitfunc(global_vars_t *gv)
{
#ifdef EXPLICIT_RELEASE
	/* あちこちに散らばってたのでまとめてみた */
#ifdef USE_MMAP
	if (gv->zz_mmap)
		apr_mmap_delete(gv->zz_mmap);
	if (gv->zz_mmap_f)
		apr_file_close(gv->zz_mmap_f);
#endif
#endif /* EXPLICIT_RELEASE */
}

#ifdef	PUT_ETAG
/* とりあえず
   ETag: "送信部のcrc32-全体のサイズ-全体のレス数-送信部のサイズ-送信部のレス数-flag"
   を%xで出力するようにしてみた。
*/
typedef struct {
	unsigned long crc;
	int allsize;
	int allres;
	int size;
	int res;
	int flag;
		/*	今のところ、
			((isbusytime) 		<< 0)
		  |	((nn_to > lineMax)	<< 1)
		*/
} etagval;

static void etag_put(const etagval *v, char *buff)
{
	sprintf(buff, "\"%lx-%x-%x-%x-%x-%x\"",
		v->crc, v->allsize, v->allres, v->size, v->res, v->flag);
}
static int etag_get(etagval *v, const char *s)
{
	return sscanf(s, "\"%lx-%x-%x-%x-%x-%x\"",
		&v->crc, &v->allsize, &v->allres, &v->size, &v->res, &v->flag) == 6;
}
static void etag_calc(const global_vars_t *gv, etagval *v)
{
	int line = gv->nn_st;
	int end = gv->nn_to;

	if (end == 0 || end > gv->lineMax)
		end = gv->lineMax;
	if (line == 0)
		line = 1;
/*	if (line > gv->lineMax)
		line = gv->lineMax;
	これをつけると、本文の出力範囲と食い違いが出る。
*/
	memset(v, 0, sizeof(*v));
	v->allsize = gv->zz_fileSize;
	v->allres = gv->lineMax;
	v->flag = (gv->isbusytime << 0) | ((gv->nn_to > gv->lineMax) << 1);
	v->crc = crc32(0, NULL, 0);

	if (!is_nofirst()) {
		v->res++;
		v->size += gv->BigLine[1] - gv->BigLine[0];
		v->crc = crc32(v->crc, gv->BigLine[0], gv->BigLine[1] - gv->BigLine[0]);
		if (line == 1)
			line++;
	}
	if (gv->isbusytime) {
		if (end - line + 1 + !is_nofirst() > RES_NORMAL)
			end = line - 1 - !is_nofirst() + RES_NORMAL;
	}
	if (line <= end) {
		v->res += end - line + 1;
		v->size += gv->BigLine[end] - gv->BigLine[line-1];
		v->crc = crc32(v->crc, gv->BigLine[line-1], gv->BigLine[end] - gv->BigLine[line-1]);
	}
}

static int need_etag(const global_vars_t *gv, request_rec *r, int st, int to, int ls)
{
	const char *env;

	if (BadAccess(gv))	/* 「なんか変です」の場合のdatの読みこみを避けるため */
		return false;

	env = r->protocol;
	if (!env || !*env)
		return false;
	if (strstr(env, "HTTP/1.1") == NULL)
		return false;
	/* ここには、If-None-Matchを付加しないUAのリスト
	   (又は付加するUAのリスト)を置き
	   無意味な場合にfalseを返すのが望ましい。 */

	/* to=nnがある場合だけ */
	if (!to)
		return false;
	return true;
}

static void create_etag(const global_vars_t *gv, char *buff)
{
	etagval val;
	etag_calc(gv, &val);
	etag_put(&val, buff);
}

static int match_etag(const global_vars_t *gv, request_rec *r, const char *etag)
{
	etagval val, qry;
	const char *oldtag;
	
	/* CHUNKの変化等が考えられ、どこで不具合が出るかもわからないので
	   当面、busytime以外は新しいものを返す */
	if (!gv->isbusytime)
		return false;
	oldtag = apr_table_get(r->subprocess_env, "HTTP_IF_NONE_MATCH");
	if (!oldtag || !*oldtag)
		return false;
	if (!etag_get(&val, etag) || !etag_get(&qry, oldtag))
		return false;

	if (val.crc != qry.crc || val.res != qry.res || val.size != qry.size)
		return false;
	
	/* 以下で、表示範囲外に追加レスがあった場合に
	   更新すべきかどうかを決める */
	
	/* 追加が100レス以内なら、同一とみなしてよい・・と思う */
	if (val.allres < qry.allres + 100)
		return true;
	
	/* キャッシュがbusytime外ものならば、そちらを優先させるべき・・と思う */
	if ((qry.flag & (1 << 0)) == 0)
		return true;
	
	/* 表示範囲が狭い場合は、CHUNK等は気にしなくてよい・・と思う */
	if (val.res < 40)
		return true;
	
	/* スレの寿命が近付いたら、警告等を表示すべき・・と思う */
	if (val.allres >= RES_YELLOW)
		return false;
	
	/* その他、迷ったらとりあえず更新せずに動作報告を待ってみよう・・と思う */
	return true;
}
#endif	/* PUT_ETAG */

/****************************************************************/
/****************************************************************/
static void header_base_out(global_vars_t *gv, request_rec *r)
{
	if ((gv->path_depth < 3 || gv->need_basehref) && gv->zz_server_name && gv->zz_script_name) {
#ifdef READ_KAKO
		if (gv->read_kako[0]) {
			ap_rprintf(r, R2CH_HTML_BASE_DEFINE, gv->zz_server_name, gv->zz_script_name, gv->zz_bs, gv->read_kako);
			gv->path_depth = 4;
		} else
#endif
		{
			ap_rprintf(r, R2CH_HTML_BASE_DEFINE, gv->zz_server_name, gv->zz_script_name, gv->zz_bs, gv->zz_ky);
			gv->path_depth = 3;
		}
	}
}

#ifdef	REFERDRES_SIMPLE
static int can_simplehtml(const global_vars_t *gv, request_rec *r)
{
	char buff[128];
	const char *p;
	const char *ref;
	const char *cginame = gv->zz_script_name;
	static const char indexname[] = "index.htm";

	if (!gv->isbusytime)
		return false;
	if (!gv->nn_st || !gv->nn_to || is_imode())
		return false;
	if (gv->nn_st > gv->nn_to || gv->nn_to > gv->lineMax)
		return false;
#if	1
	/* とりあえず、リンク先のレスが１つの場合だけ */
	if (gv->nn_st != gv->nn_to || !is_nofirst())
		return false;
#else
	if (!(gv->nn_st + 10 <= gv->nn_to))
		return false;
#endif
	ref = apr_table_get(r->subprocess_env, "HTTP_REFERER");
	if (!ref || !*ref)
		return false;

	p = strstr(ref, cginame);
	if (p) {
		p += strlen(cginame);
		if ( *p == '?' ) {
			char bbs[sizeof(gv->zz_bs)];
			char key[sizeof(gv->zz_ky)];
			const char *query_string = p+1;
#if	0
			/* 一部でREQUEST_URIがREFERERに設定されるらしいので */
			if (strcmp(gv->zz_query_string, p + 1) == 0)
				return false;
#endif
			bbs[0] = key[0] = '\0';
			GetString(query_string, bbs, sizeof(bbs), "bbs");
			GetString(query_string, key, sizeof(key), "key");
			return (strcmp(gv->zz_bs, bbs) == 0) && (strcmp(gv->zz_ky, key) == 0);
		}
		sprintf(buff, "/%.50s/%.50s/", gv->zz_bs, gv->zz_ky);
		if (!strncmp(p, buff, strlen(buff)))
			return true;
	}

	sprintf(buff, "/%.50s/", gv->zz_bs);
	p = strstr(ref, buff);
	if (p) {
		p += strlen(buff);
		if (*p == '\0')
			return true;
		if (strncmp(p, indexname, sizeof(indexname)-1) == 0)
			return true;
	}

	return false;
}

static int out_simplehtml(global_vars_t *gv, request_rec *r)
{
	int n = gv->nn_st;
	
	/* html_head() */
	ap_rputs(R2CH_HTML_HEADER_0, r);
	header_base_out(gv, r);
	ap_rprintf(r, R2CH_SIMPLE_HTML_HEADER_1("%s", ""), gv->zz_title);
	ap_rprintf(r, R2CH_HTML_HEADER_2("%s"), gv->zz_title);
	
	gv->out_resN++;	/* ヘッダ出力を抑止 */
	if (!is_nofirst()) {
		out_html(gv, r, 0, 0, 1);
		if (n == 1)
			n++;
	}
	for ( ; n <= gv->nn_to; ++n) {
		out_html(gv, r, 0, n-1, n);
	}
	
	/* html_foot() */
	/* i-mode時は来ないはずだが、もし来るようにするならPREFOOTERはimode時は出さないこと */
	ap_rputs(R2CH_HTML_PREFOOTER R2CH_HTML_FOOTER, r);
	return 0;
}
#endif	/* REFERDRES_SIMPLE */

/* 旧形式 /bbs/kako/100/1000888777.*に対応。 dokoにpathを作成する */
static int find_old_kakodir(const global_vars_t *gv, char *doko, const char *key, const char *ext)
{
	sprintf(doko, "%s" KAKO_DIR "%.3s/%.50s.%s", gv->cwd, gv->zz_bs, key, key, ext);
	return access(doko, 00) == 0;
}

static int find_kakodir(global_vars_t *gv, char *doko, const char *key, const char *ext)
{
	char *soko = gv->find_kakodir_soko;
	if (soko[0] == '\0')
		kako_dirname(soko, key);
	sprintf(doko, "%s" KAKO_DIR "%s/%.50s.%s", gv->cwd, gv->zz_bs, soko, key, ext);
	return access(doko, 00) == 0 || find_old_kakodir(gv, doko, key, ext);
}
static int find_tempdir(const global_vars_t *gv, char *doko, const char *key, const char *ext)
{
	sprintf(doko, "%s" TEMP_DIR "%.50s.%s", gv->cwd, gv->zz_bs, key, ext);
	return access(doko, 00) == 0;
}

static void create_fname(global_vars_t *gv, char *fname, const char *bbs, const char *key)
{
	/* プログラムミスによるオーバーフローの可能性を消しておく */
	gv->zz_bs[60] = gv->zz_ky[60] = '\0';
#ifdef READ_KAKO
	if (gv->read_kako[0] == 'k') {
		find_kakodir(gv, fname, key, "dat");
# ifdef READ_TEMP
	} else if (gv->read_kako[0] == 't') {
		find_tempdir(gv, fname, key, "dat");
# endif
	} else
#endif
		sprintf(fname, "%s" DAT_DIR "%.256s.dat", gv->cwd, bbs, key);

#ifdef	AUTO_KAKO
	if (gv->zz_ky[0] && access(fname, 00) != 0) {
		/* zz_kyチェックは、subject.txt取得時を除外するため */
		char buff[1024];
		const char *keybase = LastChar(key, '/');
		int mode = AUTO_KAKO_MODE;
#if	1
		/* 混雑時間帯以外のみ、temp/,kako/の閲覧を許可する場合 */
		if (gv->isbusytime)
			mode = 0;
#endif
#ifdef	RAWOUT
		if (gv->rawmode)
			mode = 2;	/* everywhere */
#endif
		if (mode >= 2 && find_kakodir(gv, buff, keybase, "dat")) {
			gv->zz_dat_where = 2;
		} else if (mode >= 1 && find_tempdir(gv, buff, keybase, "dat")) {
			gv->zz_dat_where = 1;
		}
		if (gv->zz_dat_where)
			strcpy(fname, buff);
	}
#endif	/* AUTO_KAKO */

#ifdef DEBUG
	sprintf(fname, "998695422.dat");
#endif

#ifdef	RAWOUT
	/* スレ一覧を取りに逝くモード */
	if (1 <= gv->path_depth && gv->path_depth < 3
#ifndef USE_INDEX
	&& gv->rawmode
	/* rawmodeに限り、subject.txtを返すことを可能とする。

	   ところで rawのパラメータは、通常末尾追加しか行われないことを前提とした仕組み
	   であるため、毎回全体が更新される subject.txtには適切ではない。従って現状は
	   raw=0.0 に限定して使うべきである。
	   subject.txtを読み出すモードのときには、rawのパラメータルールを変更し、
	   先頭から指定数のみを取得可能にすれば有用ではないか?
	 */ 
#endif
	) {
		sprintf(fname, "%s../%.256s/subject.txt", gv->cwd, gv->zz_bs);
#ifdef	RAWOUT_PARTIAL
		if (gv->zz_ls[0] && !gv->zz_to[0])
			strcpy(gv->zz_to, gv->zz_ls), gv->zz_ls[0] = gv->zz_st[0] = '\0';
#endif
	}
#endif
}

/****************************************************************/
/*	DSO_MAIN						*/
/****************************************************************/
int dso_main(request_rec *r, int argc, char *const *argv)
{
	global_vars_t *gv;

#ifdef USE_INDEX
	DATINDEX_OBJ dat;
#endif
	char fname[1024];
	int st, to, ls;
	apr_time_t modtime;

	if (init_global_vars(&gv, r)) return 0;
	zz_GetEnv(gv, r);

	/* st, to, lsは、このレベルでいじっておく */
	st = atoi(gv->zz_st);
	to = atoi(gv->zz_to);
	ls = atoi(gv->zz_ls);

	if (st < 0)
		st = 0;
	if (to < 0)
		to = 0;
	if (ls < 0)
		ls = 0;
	if (is_imode()) {	/* imode */
		if (!st && !to && !ls)
			ls = RES_IMODE;
	}
	/* 複数指定された時はlsを優先 */
	if (ls) {
		st = to = 0;
	}
	if (st == 1 || (to && st<=1))	/* 1を表示するのでnofirst=false */
		gv->zz_nf[0] = '\0';

	if (!is_nofirst() && ls > 0) {
		ls--;
		if(ls == 0) {
			ls = 1;
			set_nofirst_true();
		}
	}


#ifdef USE_INDEX
	/* ここでindexを読み込んでくる
	   実はすでに、.datもマッピングされちゃってるので、
	   近いウチにBigBufferは用済みになってしまう鴨 */
	if (gv->nn_ky && !datindex_open(&dat, gv->zz_bs, gv->nn_ky)) {
		ap_set_content_type(r, "text/plain");
		ap_rprintf(r, "%s/%s/%ld/error", gv->zz_bs, gv->zz_ky, gv->nn_ky);
		return 0;
	}
#endif

#ifdef RAWOUT
	if(gv->rawmode)
		/* ap_set_content_type(r, "application/octet-stream"); */
		/* 現在の.datの MIME type に合わせる．テキストデータだし... */
		ap_set_content_type(r, "text/plain");
	else
#endif
		ap_set_content_type(r, "text/html");

	create_fname(gv, fname, gv->zz_bs, gv->zz_ky);
	gv->zz_fileLastmod = getFileLastmod(gv, r, fname);
#ifdef USE_INDEX
	/* 実験仕様: 各種パラメータをいじる
	   互いに矛盾するような設定は、
	   受け入れるべきではない */
	if (st == 0 && to == 0 && ls) {
		to = dat.linenum;
		st = to - ls + 1;
		ls = 0;
	}

	/* これをやると、しばらく digest が使えなくなる */
	gv->zz_fileSize = dat.dat_stat.st_size;
	if (gv->nn_ky)
		gv->zz_fileLastmod = datindex_lastmod(&dat,
						      !is_nofirst(),
						      st,
						      to);
#endif
	modtime = gv->zz_http_if_modified_since ? apr_date_parse_rfc(gv->zz_http_if_modified_since) : APR_DATE_BAD;
	if (modtime != APR_DATE_BAD && gv->zz_fileLastmod != -1
#ifdef PREVENTRELOAD
	    /* 最後に変更してから FORCE_304_TIME 秒間は強制 304
	     * (zz_http_if_modified_since基準にすると永久に記事が取れ
	     *  ないかも)
	     */
	    && (modtime + FORCE_304_TIME * APR_USEC_PER_SEC >= gv->t_now
		|| modtime >= gv->zz_fileLastmod)) {
#else
	    && modtime >= gv->zz_fileLastmod) {
#endif
		r->status = HTTP_NOT_MODIFIED;
		return 0;
	}

#ifdef	RAWOUT
	/* この部分は、主にIf-Modified-Sinceをつけずに
	   ファイルサイズ丁度のリクエストを送って来る、
	   かちゅ～しゃによる負荷増加への対策	*/
	if (gv->rawmode) {
		/* ここまでで既にzz_fileSizeは(ファイルが見つかれば)設定されている */
		if (gv->zz_fileSize && gv->zz_fileSize == gv->raw_lastsize) {
			if (!gv->zz_http_if_modified_since) {	/* NULLだよね？KARA入れたら変えてね */
				/* 非圧縮で返す */
				char buff[120];
				ap_update_mtime(r, gv->zz_fileLastmod);
				ap_set_last_modified(r);
				/* Content-Length は Apache が自動的に付加する（はず） */
				ap_set_content_length(r, sprintf(buff, "+OK 0/%dK\n", MAX_FILESIZE / 1024));
				ap_rputs(buff, r);
				return 0;
			}
		}
#ifdef	Katjusha_DLL_REPLY
		if (gv->zz_katjusha_raw && gv->zz_fileSize) {
			if (gv->zz_fileSize < gv->raw_lastsize) {
				/* ここでhtml_error()を呼ぶと非圧縮のテキストを返すが、構わないはず。
				   Content-EncodingもLastModifiedも出力しない。*/
				char buff[120];
				ap_set_content_length(r, sprintf(buff, "-ERR %s\n", ERRORMES_ABORNED));
				ap_rputs(buff, r);
				/*html_error(gv, r, ERROR_ABORNED);*/
				return 0;
			}
		}
#endif
	}
#endif

#ifdef	PUT_ETAG
	if (need_etag(gv, r, st, to, ls)) {
		char etag[60];
		if (dat_read(gv, r, fname, st, to, ls)) goto exit;
		create_etag(gv, etag);
		if (match_etag(gv, r, etag)) {
			r->status = HTTP_NOT_MODIFIED;
			goto exit;
		}
		apr_table_set(r->headers_out, "ETag", etag);
	}
#endif

/* ここまでで、304 を返す可能性がある処理はおしまい */

	zz_init_parent_link(gv);
	zz_init_cgi_path(gv);

/*  Get Last-Modified Date */
	if (gv->zz_fileLastmod != -1) {	/* getFileLastmod のエラー時は-1が返る */
		ap_update_mtime(r, gv->zz_fileLastmod);
		ap_set_last_modified(r);
	}

	/* 通常ここでヘッダは終わり */
#ifdef DEBUG
	sleep(1);
#endif
	/* HEADリクエストならここで終了 */
	if ( is_head() )
		goto exit;

	if (logOut(gv, r, "")) goto exit;
#ifdef	RAWOUT_MULTI
	if (gv->rawmode && !*gv->zz_bs && !*gv->zz_ky && !gv->raw_lastnum && !gv->raw_lastsize && !gv->path_depth) {
		/* path_depthが１以上だと、create_fnameがsubject取得モードになってしまう */
		/* zz_bsの初期値の設定も考えられるが、Last-Modifiedの出力が嫌 */
		if (rawout_multi(gv, r))
			goto exit;	/* １つでも処理していたら、ここで終わり */
	}
#endif

	if (dat_read(gv, r, fname, st, to, ls)) goto exit;

#ifdef RAWOUT
	if (gv->rawmode) {
		dat_out_raw(gv, r);
		goto exit;
	}
#endif
#ifdef USE_INDEX
	if (gv->path_depth == 2) {
		if (gv->zz_ky[0] == '-')
			dat_out_subback();	/* スレ一覧 */
		else
			dat_out_index();	/* 板ダイジェスト */
		goto exit;
	}
#endif
#ifdef READ_KAKO
	if (gv->path_depth && gv->read_kako[0]) {
		if (gv->path_depth != 4) {
			html_error(gv, r, ERROR_NOT_FOUND);
			goto exit;
		}
	} else
#endif
	if (gv->path_depth && gv->path_depth != 3) {
		html_error(gv, r, ERROR_NOT_FOUND);
		goto exit;
	}
#ifdef	REFERDRES_SIMPLE
	if (can_simplehtml(gv, r))
		out_simplehtml(gv, r);
	else
#endif
		dat_out(gv, r, 0);
exit:	atexitfunc(gv);
	return 0;
}

#if	defined(READ_KAKO) || defined(READ_TEMP)
/* 過去ログへのリンク作成
 *
 * path_depth, zz_kyを書き換えてしまうので注意
 */
static const char *create_kako_link(global_vars_t *gv, request_rec *r, const char *dir_type, const char *key)
{
	char *result = apr_palloc(r->pool, 256);
	int needs_close_quote = false;
	char *wp = result;
	const char *p;

	*wp++ = '\"'; /* "で囲む */

	gv->path_depth = 3;
	
	if (gv->path_depth) {
		wp += sprintf(wp, "%.40s", gv->zz_script_name);
		wp += sprintf(wp, "/%.40s/%.8s/%.40s/", gv->zz_bs, dir_type, key);
	} else
		wp += sprintf(wp, "%.40s", gv->zz_cgi_path);

	sprintf(gv->zz_ky, "%.8s/%.40s", dir_type, key);
#ifdef READ_KAKO_THROUGH
	p = create_link(gv, atoi(gv->zz_st), atoi(gv->zz_to), atoi(gv->zz_ls), is_nofirst(), 0);
#else
	p = create_link(gv, 0, 0, 0, 0, 0);
#endif
	if (*p == '\"')
		++p;		/* 全体を"で囲むため、"が付いていれば取り除く */
	else
		needs_close_quote = true;	/* path形式の最後にも"を */

	if ( p[0]=='.' && p[1]=='/' )
		p += 2;	/* "./"は邪魔 */

	wp += sprintf(wp, "%.80s", p);

	if ( needs_close_quote )
		*wp++ = '\"'; /* "で囲む */
	*wp = '\0';
	return result;
}
#endif
/****************************************************************/
/*	ERROR END(html_error)					*/
/* isshowcodeとextinfoはRAWOUT時しか使わない */
/****************************************************************/
static int print_error(global_vars_t *gv, request_rec *r, enum html_error_t errorcode, int isshowcode, const char *extinfo)
{
	char tmp[256];
	char doko[256];
	char comcom[256];
	const char *mes;
	switch ( errorcode ) {
	case ERROR_TOO_HUGE:
		mes = ERRORMES_TOO_HUGE;
		break;
	case ERROR_NOT_FOUND:
		mes = ERRORMES_NOT_FOUND;
		break;
	case ERROR_NO_MEMORY:
		mes = ERRORMES_NO_MEMORY;
		break;
	case ERROR_MAINTENANCE:
		mes = ERRORMES_MAINTENANCE;
		break;
	case ERROR_LOGOUT:
		mes = ERRORMES_LOGOUT;
		break;
#ifdef	Katjusha_DLL_REPLY
	case ERROR_ABORNED:
		mes = ERRORMES_ABORNED;
		break;
#endif
	default:
		mes = "";
	}

	*tmp = '\0';
	strcpy(tmp, LastChar(gv->zz_ky, '/'));
#ifdef RAWOUT
	if(gv->rawmode) {
		/* -ERR (message)はエラー。 */
		char str[512];
		int retcode = (errorcode + 1) * 100;
		if (errorcode == ERROR_NOT_FOUND) {
			if (find_kakodir(gv, doko, tmp, "dat") || find_kakodir(gv, doko, tmp, "dat.gz")) {
				sprintf(str, ERRORMES_DAT_FOUND " %s", doko);
				mes = str;
				retcode += 1;
			} else if (find_tempdir(gv, doko, tmp, "dat")) {
				mes = ERRORMES_TEMP_FOUND;
				retcode += 2;
			}
		}
		if (isshowcode)
			ap_rprintf(r, "-ERR %d %s%s\n", retcode, mes, extinfo);
		else
			ap_rprintf(r, "-ERR %s\n", mes);
		return 0;
	}
#endif
	
	*comcom = '\0';
	if (errorcode == ERROR_LOGOUT) {
		sprintf(comcom, R2CH_HTML_ERROR_ADMIN);
	}

	ap_rprintf(r, R2CH_HTML_ERROR_1, mes, mes, mes, comcom);

	if (strstr(gv->zz_http_user_agent, "Katjusha")) {
		ap_rputs(R2CH_HTML_ERROR_2, r);
	}

	ap_rputs(R2CH_HTML_ERROR_3, r);
	html_bannerNew(r);
	ap_rputs(R2CH_HTML_ERROR_4, r);

	if (errorcode == ERROR_NOT_FOUND) {
		if (find_kakodir(gv, doko, tmp, "html")) {
			/* 過去ログ倉庫にhtml発見 */
			ap_rprintf(r, R2CH_HTML_ERROR_5_HTML, 
				   gv->zz_cgi_path, doko, tmp);
		} else if (find_kakodir(gv, doko, tmp, "dat")) {
			/* 過去ログ倉庫にdat発見 */
#ifdef READ_KAKO
 			ap_rprintf(r, R2CH_HTML_ERROR_5_DAT("%s","%s"),
				   create_kako_link("kako", tmp), tmp);
#else
			ap_rprintf(r, R2CH_HTML_ERROR_5_DAT,
				   gv->zz_cgi_path, doko, tmp);
#endif
		} else if (find_tempdir(gv, doko, tmp, "dat")) {
#ifdef READ_TEMP
			ap_rprintf(r, R2CH_HTML_ERROR_5_TEMP("%s","%s"),
				   create_kako_link("kako",tmp), tmp);
#else
			ap_rprintf(r, R2CH_HTML_ERROR_5_TEMP,
				   tmp);
#endif
		} else {
			ap_rprintf(r, R2CH_HTML_ERROR_5_NONE,
				   gv->zz_cgi_path, gv->zz_bs);
		}
	}

	ap_rputs(R2CH_HTML_ERROR_6, r);

	return 0;
}
void html_error(global_vars_t *gv, request_rec *r, enum html_error_t errorcode)
{
	print_error(gv, r, errorcode, false, "");
}
#if 0
/****************************************************************/
/*	HTML BANNER						*/
/****************************************************************/
static void html_banner(request_rec *r)
{
	ap_rputs(R2CH_HTML_BANNER, r);
}

/****************************************************************/
/*	ERROR END(html_error999)				*/
/****************************************************************/
static int html_error999(const global_vars_t *gv, request_rec *r, char const *mes)
{
	char zz_soko[256];	/* 倉庫番号(3桁数字) */
	char tmp[256];		/* ?スレ番号(9桁数字) */
	char tmp_time[16];	/* 時刻 "hh:mm:ss" */
	*tmp = '\0';
#ifdef RAWOUT
	if(gv->rawmode) {
		/* -ERR (message)はエラー。 */
		ap_rprintf(r, "-ERR %s\n", mes);
		return 0;
	}
#endif
	strcpy(tmp, LastChar(gv->zz_ky, '/'));
	kako_dirname(zz_soko, tmp);
	sprintf(tmp_time, "%02d:%02d:%02d", gv->tm_now.tm_hour, gv->tm_now.tm_min,
		gv->tm_now.tm_sec);

	ap_rprintf(r, R2CH_HTML_ERROR_999_1,
		   mes, gv->zz_bs, gv->zz_ky, gv->zz_ls, gv->zz_st, gv->zz_to, gv->zz_nf, gv->zz_fileSize,
		   gv->lineMax, tmp_time, gv->zz_bs, zz_soko, tmp, tmp);
	html_banner(r);
	ap_rputs(R2CH_HTML_ERROR_999_2, r);

	return 0;
}
#endif
/****************************************************************/
/*								*/
/****************************************************************/
#define GETSTRING_LINE_DELIM '&'
#define GETSTRING_VALUE_DELIM '='
#define MAX_QUERY_STRING 200
/* 仕様変更
   戻り値として、dstではなく(使われていないので)
   line内の次の位置を返すようにした。
   tgtが見つからない時はNULLを返す。 */
static char const *GetString(char const *line, char *dst, size_t dat_size, char const *tgt)
{
	int	i;

	char const *delim_ptr;
	char const *value_ptr;
#ifndef GSTR2
	int tgt_len = strlen(tgt);
#endif
	int line_len;
	int value_len;
	int value_start;

	for (i = 0; i < MAX_QUERY_STRING; i++) {
		/* 行末を見つける */
		delim_ptr = strchr( line, GETSTRING_LINE_DELIM );
		if ( delim_ptr )
			line_len = delim_ptr - line;
		else
			line_len = strlen(line);

		/* '='を見つける */
		value_ptr = memchr( line, GETSTRING_VALUE_DELIM, line_len );
		if ( value_ptr ) {
			value_start = value_ptr + 1 - line;
#ifdef GSTR2
			/* 最初の一文字の一致 */
			if ( *line == *tgt ) {
#else
			/* 完全一致 */
			if ( tgt_len == (value_start-1) && !memcmp(line, tgt, tgt_len) ) {
#endif
				/* 値部分の開始位置と長さ */
				value_len = line_len - value_start;

				/* 長さを丸める */
				if ( value_len >= dat_size )
					value_len = dat_size - 1;

				/* 値をコピー */
				memcpy( dst, line + value_start, value_len );
				dst[value_len] = '\0';

				return line + line_len + (delim_ptr != NULL);	/* skip delim */
			}
		}

		if ( !delim_ptr )
			break;

		line = delim_ptr + 1;	/* skip delim */
	}
	return	NULL;
}
/****************************************************************/
/*								*/
/****************************************************************/
static int IsBusy2ch(const global_vars_t *gv)
{
	return gv->tm_now.tm_hour < LIMIT_AM || LIMIT_PM <= gv->tm_now.tm_hour;
}
/****************************************************************/
/*								*/
/****************************************************************/
 /* src中の一番末尾のデリミタ文字列 c の次の文字位置を得る
  */
static char const *LastChar(char const *src, char c)
{
	char const *p;
	p = strrchr(src, c);

	if (p == NULL)
		return src;
	return p + 1;
}

#ifdef	CHUNK_ANCHOR
/* first-lastまでのCHUNKED anchorを表示
   firstとlastはレス番号。firstに0は渡すなー */
static void html_thread_anchor(global_vars_t *gv, request_rec *r, int first, int last)
{
	int line = ((first - 1)/ CHUNK_NUM) * CHUNK_NUM + 1;
	if (first <= last) {
#ifdef	CHUNKED_ANCHOR_WITH_FORM
		ap_rprintf(CHUNKED_ANCHOR_SELECT_HEAD("%s", "%s"),
			   gv->zz_bs, gv->zz_ky);
		for ( ; line <= last; line += CHUNK_NUM) {
			ap_rprintf(r, CHUNKED_ANCHOR_SELECT_STARTNUM("%d"),
				   line);
		}
		ap_rputs(CHUNKED_ANCHOR_SELECT_TAIL, r);
#else
		for ( ; line <= last; line += CHUNK_NUM) {
			ap_rprintf(r, R2CH_HTML_CHUNK_ANCHOR("%s", "%d"),
				   create_link(gv, line, 
					       line + CHUNK_NUM - 1, 
					       0,0,0),
				   line);
		}
#endif
	}
}
#else
#define	html_thread_anchor(gv, r, first, last)		/* (void)0   nothing */
#endif	/* SEPARATE_CHUNK_ANCHOR */

/* 最初と最後に表示されるレス番号を返す(レス１を除く)
   isprintedと同じ動作を。
*/
#if defined(SEPARATE_CHUNK_ANCHOR) || defined(PREV_NEXT_ANCHOR)
static int calc_first_line(const global_vars_t *gv)
{
	if (gv->nn_st)
		return gv->nn_st;
	if (gv->nn_ls)
		return gv->lineMax - gv->nn_ls + 1;
	return 1;
}
static int calc_last_line(const global_vars_t *gv)
{
	int line = gv->lineMax;

	if (gv->nn_to && gv->nn_to < gv->lineMax)
		line = gv->nn_to;
	/* 1-で計算間違ってるが、その方が都合がいい */
	if (is_imode()) {
		int imode_last = gv->first_line + RES_IMODE - 1 + is_nofirst();
		if (imode_last < line)
			line = imode_last;
	} else if (gv->isbusytime) {
		int busy_last = gv->first_line + RES_NORMAL - 1 + is_nofirst();
		if (busy_last < line)
			line = busy_last;
	}
	return line;
}

static void calc_first_last(global_vars_t *gv)
{
	/* 必ず first_line を先に計算する */
	gv->first_line = calc_first_line(gv);
	gv->last_line = calc_last_line(gv);
}
#else
#define calc_first_last()
#endif


/****************************************************************/
/*	HTML HEADER						*/
/****************************************************************/
static void html_head(global_vars_t *gv, request_rec *r, int level, char const *title, int line)
{
#ifdef USE_INDEX
	if (level) {
		ap_rprintf(r, R2CH_HTML_DIGEST_HEADER_2("%s"),
			   title);
		/* これだけ出力してもどる */
		return;
	}
#endif

	ap_rputs(R2CH_HTML_HEADER_0, r);
	header_base_out(gv, r);
	zz_init_parent_link(gv);
	zz_init_cgi_path(gv);
	calc_first_last(gv);

	if (!is_imode()) {	/* no imode       */
		ap_rprintf(r, R2CH_HTML_HEADER_1("%s", "%s"),
			   title, gv->zz_parent_link);

	/* ALL_ANCHOR は常に生きにする
	   ただし、CHUNK_ANCHORが生きで、かつisbusytimeには表示しない */
#if defined(CHUNK_ANCHOR) || defined(PREV_NEXT_ANCHOR)
		if (!gv->isbusytime)
#endif
			ap_rprintf(r, R2CH_HTML_ALL_ANCHOR("%s"),
				   create_link(gv, 0,0,0,0,0));

#if defined(PREV_NEXT_ANCHOR) && !defined(CHUNK_ANCHOR)
		ap_rprintf(r, R2CH_HTML_CHUNK_ANCHOR("%s", "1"),
			   create_link(gv, 1,CHUNK_NUM,0,0,0));
		if (gv->first_line>1) {
			ap_rprintf(r, R2CH_HTML_PREV("%s", "%d"),
				   create_link(gv, (gv->first_line<=CHUNK_NUM ? 1 : gv->first_line-CHUNK_NUM),
					       gv->first_line-1,
					       0,0,0),
				   CHUNK_NUM);
		}
		ap_rprintf(r, R2CH_HTML_NEXT("%s", "%d"),
			   create_link(gv, gv->last_line+1,
				       gv->last_line+CHUNK_NUM,0,0,0),
			   CHUNK_NUM);
#endif
#ifdef	SEPARATE_CHUNK_ANCHOR
		html_thread_anchor(gv, r, 1, gv->first_line-1);
#else
		html_thread_anchor(gv, r, 1, gv->lineMax);
#endif

		/* LATEST_ANCHORは常に */
		ap_rprintf(r, R2CH_HTML_LATEST_ANCHOR("%s", "%d"),
			   create_link(gv, 0,0, LATEST_NUM, 0,0),
			   LATEST_NUM);
	} else {
		ap_rprintf(r, R2CH_HTML_IMODE_HEADER_1("%s", "%s", "%s"),
			   title,
			   gv->zz_parent_link,
			   create_link(gv, 1,RES_IMODE, 0,0,0));
		ap_rprintf(r, R2CH_HTML_IMODE_HEADER_2("%s", "%d"),
			   create_link(gv, 0, 0, 0 /*RES_IMODE*/, 1,0),	/* imodeはスレでls=10扱い */
			   RES_IMODE);
	}

	if (line > RES_RED) {
		ap_rprintf(r, R2CH_HTML_HEADER_RED("%d"), RES_RED);
	} else if (line > RES_REDZONE) {
		ap_rprintf(r, R2CH_HTML_HEADER_REDZONE("%d", "%d"),
			   RES_REDZONE, RES_RED);
	} else if (line > RES_YELLOW) {
		ap_rprintf(r, R2CH_HTML_HEADER_YELLOW("%d", "%d"),
			   RES_YELLOW, RES_RED);
	}

#ifdef CAUTION_FILESIZE 
	if (line > RES_RED)
		;
	else
	if (gv->zz_fileSize > MAX_FILESIZE - CAUTION_FILESIZE * 1024) { 
		ap_rprintf(r, R2CH_HTML_HEADER_SIZE_REDZONE("%dKB", "%dKB", ""),
			   MAX_FILESIZE/1024 - CAUTION_FILESIZE, MAX_FILESIZE/1024); 
	} 
# ifdef MAX_FILESIZE_BUSY 
	else if (gv->zz_fileSize > MAX_FILESIZE_BUSY - CAUTION_FILESIZE * 1024) { 
		ap_rprintf(r, R2CH_HTML_HEADER_SIZE_REDZONE_TIME("%dKB", "%dKB"), 
			   MAX_FILESIZE_BUSY/1024 - CAUTION_FILESIZE, MAX_FILESIZE_BUSY/1024); 
	} 
# endif 
#endif 

	if (is_imode())
		ap_rprintf(r, R2CH_HTML_HEADER_2_I("%s"), title);
	else
		ap_rprintf(r, R2CH_HTML_HEADER_2("%s"), title);
}

/****************************************************************/
/*	RELOAD						        */
/****************************************************************/
#ifdef RELOADLINK
static void html_reload(global_vars_t *gv, request_rec *r, int startline)
{
	if (is_imode())	/*  imode */
#ifdef PREV_NEXT_ANCHOR
		return;
#else
		/* PREV_NEXTが機能を代行 */
		ap_rprintf(r, R2CH_HTML_RELOAD_I("%s"),
			   create_link(gv, startline,0, 0, 1,0));
#endif
	else {
#ifdef PREV_NEXT_ANCHOR
		if (gv->last_line < gv->lineMax) {
			if (gv->isbusytime) return;	/* 混雑時は次100にまかせる */
			ap_rprintf(r, R2CH_HTML_AFTER("%s"),
				   create_link(gv, gv->last_line+1,0, 0, 0,0));

		} else
#endif
		{
			ap_rprintf(r, R2CH_HTML_RELOAD("%s"),
				   create_link(gv, startline,0, 0, 0,0));
		}
	}
}
#endif
/****************************************************************/
/*	HTML FOOTER						*/
/****************************************************************/
static void html_foot(global_vars_t *gv, request_rec *r, int level, int line, int stopped)
{
#ifdef PREV_NEXT_ANCHOR
	int nchunk;
#endif
	/* out_resN = 0;	ダイジェスト用に再初期化 */
	if (is_imode()) {
		html_foot_im(gv, r, line, stopped);
		return;
	}
#if defined(PREV_NEXT_ANCHOR) || defined(RELOADLINK)
	ap_rputs("<hr>", r);
#endif
#ifdef PREV_NEXT_ANCHOR
	if (!gv->isbusytime) {
		ap_rprintf(r, R2CH_HTML_RETURN_BOARD("%s"),
			   gv->zz_parent_link);
		ap_rprintf(r, R2CH_HTML_ALL_ANCHOR("%s"),
			   create_link(gv, 0,0,0,0,0));
	}

#ifndef RELOADLINK
	ap_rprintf(r, R2CH_HTML_CHUNK_ANCHOR("%s", "1"),
		   create_link(gv, 1,CHUNK_NUM,0,0,0));
#endif
	if (!gv->isbusytime && gv->first_line>1) {
		ap_rprintf(r, R2CH_HTML_PREV("%s", "%d"),
			   create_link(gv, (gv->first_line<=CHUNK_NUM ? 1 : gv->first_line-CHUNK_NUM),
				       gv->first_line-1, 0,0,0),
			   CHUNK_NUM);
	}
	if (gv->isbusytime && gv->need_tail_comment)
		nchunk = RES_NORMAL;
	else
		nchunk = CHUNK_NUM;
#ifdef RELOADLINK
	if (!gv->isbusytime || gv->last_line<gv->lineMax) {
#else
	if (gv->last_line < gv->lineMax) {
#endif
		ap_rprintf(r, R2CH_HTML_NEXT("%s", "%d"),
			   create_link(gv, gv->last_line+1,
				       gv->last_line+nchunk, 0,0,0),
			   nchunk);
#ifndef RELOADLINK
	} else {
		ap_rprintf(r, R2CH_HTML_NEW("%s"),
			   create_link(gv, gv->last_line, 0, 0,0,0));
	}
#endif
#ifndef SEPARATE_CHUNK_ANCHOR
	ap_rprintf(r, R2CH_HTML_LATEST_ANCHOR("%s", "%d"),
		   create_link(gv, 0,0, LATEST_NUM, 0,0),
		   LATEST_NUM);
#endif
	if (gv->isbusytime && gv->need_tail_comment)
		ap_rprintf(r, R2CH_HTML_TAIL_SIMPLE("%02d:00", "%02d:00"),
			   LIMIT_PM - 12, LIMIT_AM);
#ifdef RELOADLINK
	}
#endif
#endif

#ifdef	SEPARATE_CHUNK_ANCHOR
#if !defined(RELOADLINK) && !defined(PREV_NEXT_ANCHOR)
	ap_rputs("<hr>", r);
#endif
	if (gv->last_line < gv->lineMax) {
		/* RELOADLINKの表示条件の逆なんだけど */
		html_thread_anchor(gv, r, gv->last_line + 1, gv->lineMax - LATEST_NUM);
		if (!(gv->isbusytime && gv->out_resN > RES_NORMAL)) {
			/* 最新レスnnがかぶるので苦肉の策
			   LATEST_ANCHORを生きにして、なおかつ末尾に持ってきているので
			   out_html内の　R2CH_HTML_TAILを修正するほうが
			   処理の流れとしては望ましいが、
			   「混雑時にCHUNK_ANCHORを非表示にする」等の場合には
			   再修正が必要なので保留 */
			/* LATEST_ANCHORも常に生きにする */
			ap_rprintf(r, R2CH_HTML_LATEST_ANCHOR("%s", "%d"),
				   create_link(gv, 0,0, LATEST_NUM, 0,0),
				   LATEST_NUM);
		}
	}
#endif
	if (line <= RES_RED && !stopped) {
		ap_rprintf(r, R2CH_HTML_FORM("%s", "%s", "%s", "%ld"),
			   gv->zz_cgi_path,
			   gv->zz_bs, gv->zz_ky, gv->currentTime);
	}

#ifdef USE_INDEX
	if (level)
		ap_rputs(R2CH_HTML_DIGEST_FOOTER, r);
	else
#endif
		ap_rputs(R2CH_HTML_FOOTER, r);
}
/****************************************************************/
/*	HTML FOOTER(i-MODE)					*/
/****************************************************************/
static void html_foot_im(global_vars_t *gv, request_rec *r, int line, int stopped)
{
#ifdef PREV_NEXT_ANCHOR
	if (gv->last_line < gv->lineMax) {
		ap_rprintf(r, R2CH_HTML_NEXT("%s", "%d"),
			   create_link(gv, gv->last_line+1,
				       gv->last_line+RES_IMODE, 0,1,0),
			   RES_IMODE);
	} else {
		ap_rprintf(r, R2CH_HTML_NEW_I("%s"),
			   create_link(gv, gv->last_line,
				       gv->last_line+RES_IMODE-1, 0,1,0));
	}
	if (gv->first_line > 1) {
		ap_rprintf(r, R2CH_HTML_PREV("%s", "%d"),
			   create_link(gv, (gv->first_line<=RES_IMODE ? 1 : gv->first_line-RES_IMODE),
			   gv->first_line-1, 0,1,0),
			   RES_IMODE);
	}
	ap_rprintf(r, R2CH_HTML_IMODE_TAIL2("%s", "%d"),
		   create_link(gv, 0, 0, 0 /*RES_IMODE*/, 1,0),	/* imodeはスレでls=10扱い */
		   RES_IMODE);
#endif
	if (line <= RES_RED && !stopped) {
		ap_rprintf(r, R2CH_HTML_FORM_IMODE("%s", "%s", "%s", "%ld"),
			   gv->zz_cgi_path,
			   gv->zz_bs, gv->zz_ky, gv->currentTime); 
	}
	ap_rputs(R2CH_HTML_FOOTER_IMODE, r);
}

/****************************************************************/
/*	過去ログpath生成					*/
/****************************************************************/
static void kako_dirname(char *buf, const char *key)
{
	int tm = atoi(key) / 100000;
	if (tm < 10000) {
	    /*  9aabbbbbb -> 9aa */
	    sprintf(buf, "%03d", tm / 10);
	} else {
	    /* 1aaabccccc -> 1aaa/1aaab */
	    sprintf(buf, "%d/%d", tm / 10, tm);
	}
}

/****************************************************************/
/*	END OF THIS FILE					*/
/****************************************************************/
