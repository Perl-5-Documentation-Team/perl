/* $Header: str.c,v 3.0 89/10/18 15:23:38 lwall Locked $
 *
 *    Copyright (c) 1989, Larry Wall
 *
 *    You may distribute under the terms of the GNU General Public License
 *    as specified in the README file that comes with the perl 3.0 kit.
 *
 * $Log:	str.c,v $
 * Revision 3.0  89/10/18  15:23:38  lwall
 * 3.0 baseline
 * 
 */

#include "EXTERN.h"
#include "perl.h"
#include "perly.h"

extern char **environ;

#ifndef str_get
char *
str_get(str)
STR *str;
{
#ifdef TAINT
    tainted |= str->str_tainted;
#endif
    return str->str_pok ? str->str_ptr : str_2ptr(str);
}
#endif

/* dlb ... guess we have a "crippled cc".
 * dlb the following functions are usually macros.
 */
#ifndef str_true
str_true(Str)
STR *Str;
{
	if (Str->str_pok) {
	    if (*Str->str_ptr > '0' ||
	      Str->str_cur > 1 ||
	      (Str->str_cur && *Str->str_ptr != '0'))
		return 1;
	    return 0;
	}
	if (Str->str_nok)
		return (Str->str_u.str_nval != 0.0);
	return 0;
}
#endif /* str_true */

#ifndef str_gnum
double str_gnum(Str)
STR *Str;
{
#ifdef TAINT
	tainted |= Str->str_tainted;
#endif /* TAINT*/
	if (Str->str_nok)
		return Str->str_u.str_nval;
	return str_2num(Str);
}
#endif /* str_gnum */
/* dlb ... end of crutch */

char *
str_grow(str,newlen)
register STR *str;
register int newlen;
{
    register char *s = str->str_ptr;

    if (str->str_state == SS_INCR) {		/* data before str_ptr? */
	str->str_len += str->str_u.str_useful;
	str->str_ptr -= str->str_u.str_useful;
	str->str_u.str_useful = 0L;
	bcopy(s, str->str_ptr, str->str_cur+1);
	s = str->str_ptr;
	str->str_state = SS_NORM;			/* normal again */
	if (newlen > str->str_len)
	    newlen += 10 * (newlen - str->str_cur); /* avoid copy each time */
    }
    if (newlen > str->str_len) {		/* need more room? */
        if (str->str_len)
	    Renew(s,newlen,char);
        else
	    New(703,s,newlen,char);
	str->str_ptr = s;
        str->str_len = newlen;
    }
    return s;
}

str_numset(str,num)
register STR *str;
double num;
{
    str->str_u.str_nval = num;
    str->str_state = SS_NORM;
    str->str_pok = 0;	/* invalidate pointer */
    str->str_nok = 1;			/* validate number */
#ifdef TAINT
    str->str_tainted = tainted;
#endif
}

extern int errno;

char *
str_2ptr(str)
register STR *str;
{
    register char *s;
    int olderrno;

    if (!str)
	return "";
    if (str->str_nok) {
	STR_GROW(str, 24);
	s = str->str_ptr;
	olderrno = errno;	/* some Xenix systems wipe out errno here */
#if defined(scs) && defined(ns32000)
	gcvt(str->str_u.str_nval,20,s);
#else
#ifdef apollo
	if (str->str_u.str_nval == 0.0)
	    (void)strcpy(s,"0");
	else
#endif /*apollo*/
	(void)sprintf(s,"%.20g",str->str_u.str_nval);
#endif /*scs*/
	errno = olderrno;
	while (*s) s++;
    }
    else {
	if (str == &str_undef)
	    return No;
	if (dowarn)
	    warn("Use of uninitialized variable");
	STR_GROW(str, 24);
	s = str->str_ptr;
    }
    *s = '\0';
    str->str_cur = s - str->str_ptr;
    str->str_pok = 1;
#ifdef DEBUGGING
    if (debug & 32)
	fprintf(stderr,"0x%lx ptr(%s)\n",str,str->str_ptr);
#endif
    return str->str_ptr;
}

double
str_2num(str)
register STR *str;
{
    if (!str)
	return 0.0;
    str->str_state = SS_NORM;
    if (str->str_len && str->str_pok)
	str->str_u.str_nval = atof(str->str_ptr);
    else  {
	if (str == &str_undef)
	    return 0.0;
	if (dowarn)
	    warn("Use of uninitialized variable");
	str->str_u.str_nval = 0.0;
    }
    str->str_nok = 1;
#ifdef DEBUGGING
    if (debug & 32)
	fprintf(stderr,"0x%lx num(%g)\n",str,str->str_u.str_nval);
#endif
    return str->str_u.str_nval;
}

str_sset(dstr,sstr)
STR *dstr;
register STR *sstr;
{
#ifdef TAINT
    tainted |= sstr->str_tainted;
#endif
    if (!sstr)
	dstr->str_pok = dstr->str_nok = 0;
    else if (sstr->str_pok) {
	str_nset(dstr,sstr->str_ptr,sstr->str_cur);
	if (sstr->str_nok) {
	    dstr->str_u.str_nval = sstr->str_u.str_nval;
	    dstr->str_nok = 1;
	    dstr->str_state = SS_NORM;
	}
	else if (sstr->str_cur == sizeof(STBP)) {
	    char *tmps = sstr->str_ptr;

	    if (*tmps == 'S' && bcmp(tmps,"Stab",4) == 0) {
		dstr->str_magic = str_smake(sstr->str_magic);
		dstr->str_magic->str_rare = 'X';
	    }
	}
    }
    else if (sstr->str_nok)
	str_numset(dstr,sstr->str_u.str_nval);
    else
	dstr->str_pok = dstr->str_nok = 0;
}

str_nset(str,ptr,len)
register STR *str;
register char *ptr;
register int len;
{
    STR_GROW(str, len + 1);
    (void)bcopy(ptr,str->str_ptr,len);
    str->str_cur = len;
    *(str->str_ptr+str->str_cur) = '\0';
    str->str_nok = 0;		/* invalidate number */
    str->str_pok = 1;		/* validate pointer */
#ifdef TAINT
    str->str_tainted = tainted;
#endif
}

str_set(str,ptr)
register STR *str;
register char *ptr;
{
    register int len;

    if (!ptr)
	ptr = "";
    len = strlen(ptr);
    STR_GROW(str, len + 1);
    (void)bcopy(ptr,str->str_ptr,len+1);
    str->str_cur = len;
    str->str_nok = 0;		/* invalidate number */
    str->str_pok = 1;		/* validate pointer */
#ifdef TAINT
    str->str_tainted = tainted;
#endif
}

str_chop(str,ptr)	/* like set but assuming ptr is in str */
register STR *str;
register char *ptr;
{
    register int delta;

    if (!(str->str_pok))
	fatal("str_chop: internal inconsistency");
    delta = ptr - str->str_ptr;
    str->str_len -= delta;
    str->str_cur -= delta;
    str->str_ptr += delta;
    if (str->str_state == SS_INCR)
	str->str_u.str_useful += delta;
    else {
	str->str_u.str_useful = delta;
	str->str_state = SS_INCR;
    }
    str->str_nok = 0;		/* invalidate number */
    str->str_pok = 1;		/* validate pointer (and unstudy str) */
}

str_ncat(str,ptr,len)
register STR *str;
register char *ptr;
register int len;
{
    if (!(str->str_pok))
	(void)str_2ptr(str);
    STR_GROW(str, str->str_cur + len + 1);
    (void)bcopy(ptr,str->str_ptr+str->str_cur,len);
    str->str_cur += len;
    *(str->str_ptr+str->str_cur) = '\0';
    str->str_nok = 0;		/* invalidate number */
    str->str_pok = 1;		/* validate pointer */
#ifdef TAINT
    str->str_tainted |= tainted;
#endif
}

str_scat(dstr,sstr)
STR *dstr;
register STR *sstr;
{
#ifdef TAINT
    tainted |= sstr->str_tainted;
#endif
    if (!sstr)
	return;
    if (!(sstr->str_pok))
	(void)str_2ptr(sstr);
    if (sstr)
	str_ncat(dstr,sstr->str_ptr,sstr->str_cur);
}

str_cat(str,ptr)
register STR *str;
register char *ptr;
{
    register int len;

    if (!ptr)
	return;
    if (!(str->str_pok))
	(void)str_2ptr(str);
    len = strlen(ptr);
    STR_GROW(str, str->str_cur + len + 1);
    (void)bcopy(ptr,str->str_ptr+str->str_cur,len+1);
    str->str_cur += len;
    str->str_nok = 0;		/* invalidate number */
    str->str_pok = 1;		/* validate pointer */
#ifdef TAINT
    str->str_tainted |= tainted;
#endif
}

char *
str_append_till(str,from,fromend,delim,keeplist)
register STR *str;
register char *from;
register char *fromend;
register int delim;
char *keeplist;
{
    register char *to;
    register int len;

    if (!from)
	return Nullch;
    len = fromend - from;
    STR_GROW(str, str->str_cur + len + 1);
    str->str_nok = 0;		/* invalidate number */
    str->str_pok = 1;		/* validate pointer */
    to = str->str_ptr+str->str_cur;
    for (; from < fromend; from++,to++) {
	if (*from == '\\' && from+1 < fromend && delim != '\\') {
	    if (!keeplist) {
		if (from[1] == delim || from[1] == '\\')
		    from++;
		else
		    *to++ = *from++;
	    }
	    else if (from[1] && index(keeplist,from[1]))
		*to++ = *from++;
	    else
		from++;
	}
	else if (*from == delim)
	    break;
	*to = *from;
    }
    *to = '\0';
    str->str_cur = to - str->str_ptr;
    return from;
}

STR *
#ifdef LEAKTEST
str_new(x,len)
int x;
#else
str_new(len)
#endif
int len;
{
    register STR *str;
    
    if (freestrroot) {
	str = freestrroot;
	freestrroot = str->str_magic;
	str->str_magic = Nullstr;
	str->str_state = SS_NORM;
    }
    else {
	Newz(700+x,str,1,STR);
    }
    if (len)
	STR_GROW(str, len + 1);
    return str;
}

void
str_magic(str, stab, how, name, namlen)
register STR *str;
STAB *stab;
int how;
char *name;
int namlen;
{
    if (str->str_magic)
	return;
    str->str_magic = Str_new(75,namlen);
    str = str->str_magic;
    str->str_u.str_stab = stab;
    str->str_rare = how;
    if (name)
	str_nset(str,name,namlen);
}

void
str_insert(bigstr,offset,len,little,littlelen)
STR *bigstr;
int offset;
int len;
char *little;
int littlelen;
{
    register char *big;
    register char *mid;
    register char *midend;
    register char *bigend;
    register int i;

    i = littlelen - len;
    if (i > 0) {			/* string might grow */
	STR_GROW(bigstr, bigstr->str_cur + i + 1);
	big = bigstr->str_ptr;
	mid = big + offset + len;
	midend = bigend = big + bigstr->str_cur;
	bigend += i;
	*bigend = '\0';
	while (midend > mid)		/* shove everything down */
	    *--bigend = *--midend;
	(void)bcopy(little,big+offset,littlelen);
	bigstr->str_cur += i;
	return;
    }
    else if (i == 0) {
	(void)bcopy(little,bigstr->str_ptr+offset,len);
	return;
    }

    big = bigstr->str_ptr;
    mid = big + offset;
    midend = mid + len;
    bigend = big + bigstr->str_cur;

    if (midend > bigend)
	fatal("panic: str_insert");

    bigstr->str_pok = SP_VALID;	/* disable possible screamer */

    if (mid - big > bigend - midend) {	/* faster to shorten from end */
	if (littlelen) {
	    (void)bcopy(little, mid, littlelen);
	    mid += littlelen;
	}
	i = bigend - midend;
	if (i > 0) {
	    (void)bcopy(midend, mid, i);
	    mid += i;
	}
	*mid = '\0';
	bigstr->str_cur = mid - big;
    }
    else if (i = mid - big) {	/* faster from front */
	midend -= littlelen;
	mid = midend;
	str_chop(bigstr,midend-i);
	big += i;
	while (i--)
	    *--midend = *--big;
	if (littlelen)
	    (void)bcopy(little, mid, littlelen);
    }
    else if (littlelen) {
	midend -= littlelen;
	str_chop(bigstr,midend);
	(void)bcopy(little,midend,littlelen);
    }
    else {
	str_chop(bigstr,midend);
    }
    STABSET(bigstr);
}

/* make str point to what nstr did */

void
str_replace(str,nstr)
register STR *str;
register STR *nstr;
{
    if (str->str_state == SS_INCR)
	str_grow(str,0);	/* just force copy down */
    if (nstr->str_state == SS_INCR)
	str_grow(nstr,0);
    if (str->str_ptr)
	Safefree(str->str_ptr);
    str->str_ptr = nstr->str_ptr;
    str->str_len = nstr->str_len;
    str->str_cur = nstr->str_cur;
    str->str_pok = nstr->str_pok;
    str->str_nok = nstr->str_nok;
#ifdef STRUCTCOPY
    str->str_u = nstr->str_u;
#else
    str->str_u.str_nval = nstr->str_u.str_nval;
#endif
#ifdef TAINT
    str->str_tainted = nstr->str_tainted;
#endif
    Safefree(nstr);
}

void
str_free(str)
register STR *str;
{
    if (!str)
	return;
    if (str->str_state) {
	if (str->str_state == SS_FREE)	/* already freed */
	    return;
	if (str->str_state == SS_INCR && !(str->str_pok & 2)) {
	    str->str_ptr -= str->str_u.str_useful;
	    str->str_len += str->str_u.str_useful;
	}
    }
    if (str->str_magic)
	str_free(str->str_magic);
#ifdef LEAKTEST
    if (str->str_len)
	Safefree(str->str_ptr);
    if ((str->str_pok & SP_INTRP) && str->str_u.str_args)
	arg_free(str->str_u.str_args);
    Safefree(str);
#else /* LEAKTEST */
    if (str->str_len) {
	if (str->str_len > 127) {	/* next user not likely to want more */
	    Safefree(str->str_ptr);	/* so give it back to malloc */
	    str->str_ptr = Nullch;
	    str->str_len = 0;
	}
	else
	    str->str_ptr[0] = '\0';
    }
    if ((str->str_pok & SP_INTRP) && str->str_u.str_args)
	arg_free(str->str_u.str_args);
    str->str_cur = 0;
    str->str_nok = 0;
    str->str_pok = 0;
    str->str_state = SS_FREE;
#ifdef TAINT
    str->str_tainted = 0;
#endif
    str->str_magic = freestrroot;
    freestrroot = str;
#endif /* LEAKTEST */
}

str_len(str)
register STR *str;
{
    if (!str)
	return 0;
    if (!(str->str_pok))
	(void)str_2ptr(str);
    if (str->str_ptr)
	return str->str_cur;
    else
	return 0;
}

str_eq(str1,str2)
register STR *str1;
register STR *str2;
{
    if (!str1)
	return str2 == Nullstr;
    if (!str2)
	return 0;

    if (!str1->str_pok)
	(void)str_2ptr(str1);
    if (!str2->str_pok)
	(void)str_2ptr(str2);

    if (str1->str_cur != str2->str_cur)
	return 0;

    return !bcmp(str1->str_ptr, str2->str_ptr, str1->str_cur);
}

str_cmp(str1,str2)
register STR *str1;
register STR *str2;
{
    int retval;

    if (!str1)
	return str2 == Nullstr;
    if (!str2)
	return 0;

    if (!str1->str_pok)
	(void)str_2ptr(str1);
    if (!str2->str_pok)
	(void)str_2ptr(str2);

    if (str1->str_cur < str2->str_cur) {
	if (retval = memcmp(str1->str_ptr, str2->str_ptr, str1->str_cur))
	    return retval;
	else
	    return 1;
    }
    else if (retval = memcmp(str1->str_ptr, str2->str_ptr, str2->str_cur))
	return retval;
    else if (str1->str_cur == str2->str_cur)
	return 0;
    else
	return -1;
}

char *
str_gets(str,fp,append)
register STR *str;
register FILE *fp;
int append;
{
#ifdef STDSTDIO		/* Here is some breathtakingly efficient cheating */

    register char *bp;		/* we're going to steal some values */
    register int cnt;		/*  from the stdio struct and put EVERYTHING */
    register STDCHAR *ptr;	/*   in the innermost loop into registers */
    register char newline = record_separator;/* (assuming >= 6 registers) */
    int i;
    int bpx;
    int obpx;
    register int get_paragraph;
    register char *oldbp;

    if (get_paragraph = !rslen) {	/* yes, that's an assignment */
	newline = '\n';
	oldbp = Nullch;			/* remember last \n position (none) */
    }
    cnt = fp->_cnt;			/* get count into register */
    str->str_nok = 0;			/* invalidate number */
    str->str_pok = 1;			/* validate pointer */
    if (str->str_len <= cnt + 1)	/* make sure we have the room */
	STR_GROW(str, append+cnt+2);	/* (remembering cnt can be -1) */
    bp = str->str_ptr + append;		/* move these two too to registers */
    ptr = fp->_ptr;
    for (;;) {
      screamer:
	while (--cnt >= 0) {			/* this */	/* eat */
	    if ((*bp++ = *ptr++) == newline)	/* really */	/* dust */
		goto thats_all_folks;		/* screams */	/* sed :-) */ 
	}
	
	fp->_cnt = cnt;			/* deregisterize cnt and ptr */
	fp->_ptr = ptr;
	i = _filbuf(fp);		/* get more characters */
	cnt = fp->_cnt;
	ptr = fp->_ptr;			/* reregisterize cnt and ptr */

	bpx = bp - str->str_ptr;	/* prepare for possible relocation */
	if (get_paragraph && oldbp)
	    obpx = oldbp - str->str_ptr;
	STR_GROW(str, bpx + cnt + 2);
	bp = str->str_ptr + bpx;	/* reconstitute our pointer */
	if (get_paragraph && oldbp)
	    oldbp = str->str_ptr + obpx;

	if (i == newline) {		/* all done for now? */
	    *bp++ = i;
	    goto thats_all_folks;
	}
	else if (i == EOF)		/* all done for ever? */
	    goto thats_really_all_folks;
	*bp++ = i;			/* now go back to screaming loop */
    }

thats_all_folks:
    if (get_paragraph && bp - 1 != oldbp) {
	oldbp = bp;	/* remember where this newline was */
	goto screamer;	/* and go back to the fray */
    }
thats_really_all_folks:
    fp->_cnt = cnt;			/* put these back or we're in trouble */
    fp->_ptr = ptr;
    *bp = '\0';
    str->str_cur = bp - str->str_ptr;	/* set length */

#else /* !STDSTDIO */	/* The big, slow, and stupid way */

    static char buf[8192];

    if (fgets(buf, sizeof buf, fp) != Nullch) {
	if (append)
	    str_cat(str, buf);
	else
	    str_set(str, buf);
    }
    else
	str_set(str, No);

#endif /* STDSTDIO */

    return str->str_cur - append ? str->str_ptr : Nullch;
}

ARG *
parselist(str)
STR *str;
{
    register CMD *cmd;
    register ARG *arg;
    line_t oldline = line;
    int retval;

    str_sset(linestr,str);
    in_eval++;
    oldoldbufptr = oldbufptr = bufptr = str_get(linestr);
    bufend = bufptr + linestr->str_cur;
    if (setjmp(eval_env)) {
	in_eval = 0;
	fatal("%s\n",stab_val(stabent("@",TRUE))->str_ptr);
    }
    error_count = 0;
    retval = yyparse();
    in_eval--;
    if (retval || error_count)
	fatal("Invalid component in string or format");
    cmd = eval_root;
    arg = cmd->c_expr;
    if (cmd->c_type != C_EXPR || cmd->c_next || arg->arg_type != O_LIST)
	fatal("panic: error in parselist %d %x %d", cmd->c_type,
	  cmd->c_next, arg ? arg->arg_type : -1);
    line = oldline;
    Safefree(cmd);
    return arg;
}

void
intrpcompile(src)
STR *src;
{
    register char *s = str_get(src);
    register char *send = s + src->str_cur;
    register STR *str;
    register char *t;
    STR *toparse;
    int len;
    register int brackets;
    register char *d;
    STAB *stab;
    char *checkpoint;

    toparse = Str_new(76,0);
    str = Str_new(77,0);

    str_nset(str,"",0);
    str_nset(toparse,"",0);
    t = s;
    while (s < send) {
	if (*s == '\\' && s[1] && index("$@[{\\]}",s[1])) {
	    str_ncat(str, t, s - t);
	    ++s;
	    if (*nointrp && s+1 < send)
		if (*s != '@' && (*s != '$' || index(nointrp,s[1])))
		    str_ncat(str,s-1,1);
	    str_ncat(str, "$b", 2);
	    str_ncat(str, s, 1);
	    ++s;
	    t = s;
	}
	else if ((*s == '@' || (*s == '$' && !index(nointrp,s[1]))) &&
	  s+1 < send) {
	    str_ncat(str,t,s-t);
	    t = s;
	    if (*s == '$' && s[1] == '#' && isalpha(s[2]) || s[2] == '_')
		s++;
	    s = scanreg(s,send,tokenbuf);
	    if (*t == '@' &&
	      (!(stab = stabent(tokenbuf,FALSE)) || !stab_xarray(stab)) ) {
		str_ncat(str,"@",1);
		s = ++t;
		continue;	/* grandfather @ from old scripts */
	    }
	    str_ncat(str,"$a",2);
	    str_ncat(toparse,",",1);
	    if (t[1] != '{' && (*s == '['  || *s == '{' /* }} */ ) &&
	      (stab = stabent(tokenbuf,FALSE)) &&
	      ((*s == '[') ? (stab_xarray(stab) != 0) : (stab_xhash(stab) != 0)) ) {
		brackets = 0;
		checkpoint = s;
		do {
		    switch (*s) {
		    case '[': case '{':
			brackets++;
			break;
		    case ']': case '}':
			brackets--;
			break;
		    case '\'':
		    case '"':
			if (s[-1] != '$') {
			    s = cpytill(tokenbuf,s+1,send,*s,&len);
			    if (s >= send)
				fatal("Unterminated string");
			}
			break;
		    }
		    s++;
		} while (brackets > 0 && s < send);
		if (s > send)
		    fatal("Unmatched brackets in string");
		if (*nointrp) {		/* we're in a regular expression */
		    d = checkpoint;
		    if (*d == '{' && s[-1] == '}') {	/* maybe {n,m} */
			++d;
			if (isdigit(*d)) {	/* matches /^{\d,?\d*}$/ */
			    if (*++d == ',')
				++d;
			    while (isdigit(*d))
				d++;
			    if (d == s - 1)
				s = checkpoint;		/* Is {n,m}! Backoff! */
			}
		    }
		    else if (*d == '[' && s[-1] == ']') { /* char class? */
			int weight = 2;		/* let's weigh the evidence */
			char seen[256];
			unsigned char uchar = 0, lastuchar;

			Zero(seen,256,char);
			*--s = '\0';
			if (d[1] == '^')
			    weight += 150;
			else if (d[1] == '$')
			    weight -= 3;
			if (isdigit(d[1])) {
			    if (d[2]) {
				if (isdigit(d[2]) && !d[3])
				    weight -= 10;
			    }
			    else
				weight -= 100;
			}
			for (d++; d < s; d++) {
			    lastuchar = uchar;
			    uchar = (unsigned char)*d;
			    switch (*d) {
			    case '&':
			    case '$':
				weight -= seen[uchar] * 10;
				if (isalpha(d[1]) || isdigit(d[1]) ||
				  d[1] == '_') {
				    d = scanreg(d,s,tokenbuf);
				    if (stabent(tokenbuf,FALSE))
					weight -= 100;
				    else
					weight -= 10;
				}
				else if (*d == '$' && d[1] &&
				  index("[#!%*<>()-=",d[1])) {
				    if (!d[2] || /*{*/ index("])} =",d[2]))
					weight -= 10;
				    else
					weight -= 1;
				}
				break;
			    case '\\':
				uchar = 254;
				if (d[1]) {
				    if (index("wds",d[1]))
					weight += 100;
				    else if (seen['\''] || seen['"'])
					weight += 1;
				    else if (index("rnftb",d[1]))
					weight += 40;
				    else if (isdigit(d[1])) {
					weight += 40;
					while (d[1] && isdigit(d[1]))
					    d++;
				    }
				}
				else
				    weight += 100;
				break;
			    case '-':
				if (lastuchar < d[1] || d[1] == '\\') {
				    if (index("aA01! ",lastuchar))
					weight += 30;
				    if (index("zZ79~",d[1]))
					weight += 30;
				}
				else
				    weight -= 1;
			    default:
				if (isalpha(*d) && d[1] && isalpha(d[1])) {
				    bufptr = d;
				    if (yylex() != WORD)
					weight -= 150;
				    d = bufptr;
				}
				if (uchar == lastuchar + 1)
				    weight += 5;
				weight -= seen[uchar];
				break;
			    }
			    seen[uchar]++;
			}
#ifdef DEBUGGING
			if (debug & 512)
			    fprintf(stderr,"[%s] weight %d\n",
			      checkpoint+1,weight);
#endif
			*s++ = ']';
			if (weight >= 0)	/* probably a character class */
			    s = checkpoint;
		    }
		}
	    }
	    if (*t == '@')
		str_ncat(toparse, "join($\",", 8);
	    if (t[1] == '{' && s[-1] == '}') {
		str_ncat(toparse, t, 1);
		str_ncat(toparse, t+2, s - t - 3);
	    }
	    else
		str_ncat(toparse, t, s - t);
	    if (*t == '@')
		str_ncat(toparse, ")", 1);
	    t = s;
	}
	else
	    s++;
    }
    str_ncat(str,t,s-t);
    if (toparse->str_ptr && *toparse->str_ptr == ',') {
	*toparse->str_ptr = '(';
	str_ncat(toparse,",$$);",5);
	str->str_u.str_args = parselist(toparse);
	str->str_u.str_args->arg_len--;		/* ignore $$ reference */
    }
    else
	str->str_u.str_args = Nullarg;
    str_free(toparse);
    str->str_pok |= SP_INTRP;
    str->str_nok = 0;
    str_replace(src,str);
}

STR *
interp(str,src,sp)
register STR *str;
STR *src;
int sp;
{
    register char *s;
    register char *t;
    register char *send;
    register STR **elem;

    if (!(src->str_pok & SP_INTRP)) {
	int oldsave = savestack->ary_fill;

	(void)savehptr(&curstash);
	curstash = src->str_u.str_hash;	/* so stabent knows right package */
	intrpcompile(src);
	restorelist(oldsave);
    }
    s = src->str_ptr;		/* assumed valid since str_pok set */
    t = s;
    send = s + src->str_cur;

    if (src->str_u.str_args) {
	(void)eval(src->str_u.str_args,G_ARRAY,sp);
	/* Assuming we have correct # of args */
	elem = stack->ary_array + sp;
    }

    str_nset(str,"",0);
    while (s < send) {
	if (*s == '$' && s+1 < send) {
	    str_ncat(str,t,s-t);
	    switch(*++s) {
	    case 'a':
		str_scat(str,*++elem);
		break;
	    case 'b':
		str_ncat(str,++s,1);
		break;
	    }
	    t = ++s;
	}
	else
	    s++;
    }
    str_ncat(str,t,s-t);
    return str;
}

void
str_inc(str)
register STR *str;
{
    register char *d;

    if (!str)
	return;
    if (str->str_nok) {
	str->str_u.str_nval += 1.0;
	str->str_pok = 0;
	return;
    }
    if (!str->str_pok || !*str->str_ptr) {
	str->str_u.str_nval = 1.0;
	str->str_nok = 1;
	str->str_pok = 0;
	return;
    }
    d = str->str_ptr;
    while (isalpha(*d)) d++;
    while (isdigit(*d)) d++;
    if (*d) {
        str_numset(str,atof(str->str_ptr) + 1.0);  /* punt */
	return;
    }
    d--;
    while (d >= str->str_ptr) {
	if (isdigit(*d)) {
	    if (++*d <= '9')
		return;
	    *(d--) = '0';
	}
	else {
	    ++*d;
	    if (isalpha(*d))
		return;
	    *(d--) -= 'z' - 'a' + 1;
	}
    }
    /* oh,oh, the number grew */
    STR_GROW(str, str->str_cur + 2);
    str->str_cur++;
    for (d = str->str_ptr + str->str_cur; d > str->str_ptr; d--)
	*d = d[-1];
    if (isdigit(d[1]))
	*d = '1';
    else
	*d = d[1];
}

void
str_dec(str)
register STR *str;
{
    if (!str)
	return;
    if (str->str_nok) {
	str->str_u.str_nval -= 1.0;
	str->str_pok = 0;
	return;
    }
    if (!str->str_pok) {
	str->str_u.str_nval = -1.0;
	str->str_nok = 1;
	return;
    }
    str_numset(str,atof(str->str_ptr) - 1.0);
}

/* Make a string that will exist for the duration of the expression
 * evaluation.  Actually, it may have to last longer than that, but
 * hopefully cmd_exec won't free it until it has been assigned to a
 * permanent location. */

static long tmps_size = -1;

STR *
str_static(oldstr)
STR *oldstr;
{
    register STR *str = Str_new(78,0);

    str_sset(str,oldstr);
    if (++tmps_max > tmps_size) {
	tmps_size = tmps_max;
	if (!(tmps_size & 127)) {
	    if (tmps_size)
		Renew(tmps_list, tmps_size + 128, STR*);
	    else
		New(702,tmps_list, 128, STR*);
	}
    }
    tmps_list[tmps_max] = str;
    return str;
}

/* same thing without the copying */

STR *
str_2static(str)
register STR *str;
{
    if (++tmps_max > tmps_size) {
	tmps_size = tmps_max;
	if (!(tmps_size & 127)) {
	    if (tmps_size)
		Renew(tmps_list, tmps_size + 128, STR*);
	    else
		New(704,tmps_list, 128, STR*);
	}
    }
    tmps_list[tmps_max] = str;
    return str;
}

STR *
str_make(s,len)
char *s;
int len;
{
    register STR *str = Str_new(79,0);

    if (!len)
	len = strlen(s);
    str_nset(str,s,len);
    return str;
}

STR *
str_nmake(n)
double n;
{
    register STR *str = Str_new(80,0);

    str_numset(str,n);
    return str;
}

/* make an exact duplicate of old */

STR *
str_smake(old)
register STR *old;
{
    register STR *new = Str_new(81,0);

    if (!old)
	return Nullstr;
    if (old->str_state == SS_FREE) {
	warn("semi-panic: attempt to dup freed string");
	return Nullstr;
    }
    if (old->str_state == SS_INCR && !(old->str_pok & 2))
	str_grow(old,0);
    if (new->str_ptr)
	Safefree(new->str_ptr);
    Copy(old,new,1,STR);
    if (old->str_ptr)
	new->str_ptr = nsavestr(old->str_ptr,old->str_len);
    return new;
}

str_reset(s,stash)
register char *s;
HASH *stash;
{
    register HENT *entry;
    register STAB *stab;
    register STR *str;
    register int i;
    register SPAT *spat;
    register int max;

    if (!*s) {		/* reset ?? searches */
	for (spat = stash->tbl_spatroot;
	  spat != Nullspat;
	  spat = spat->spat_next) {
	    spat->spat_flags &= ~SPAT_USED;
	}
	return;
    }

    /* reset variables */

    while (*s) {
	i = *s;
	if (s[1] == '-') {
	    s += 2;
	}
	max = *s++;
	for ( ; i <= max; i++) {
	    for (entry = stash->tbl_array[i];
	      entry;
	      entry = entry->hent_next) {
		stab = (STAB*)entry->hent_val;
		str = stab_val(stab);
		str->str_cur = 0;
		str->str_nok = 0;
#ifdef TAINT
		str->str_tainted = tainted;
#endif
		if (str->str_ptr != Nullch)
		    str->str_ptr[0] = '\0';
		if (stab_xarray(stab)) {
		    aclear(stab_xarray(stab));
		}
		if (stab_xhash(stab)) {
		    hclear(stab_xhash(stab));
		    if (stab == envstab)
			environ[0] = Nullch;
		}
	    }
	}
    }
}

#ifdef TAINT
taintproper(s)
char *s;
{
#ifdef DEBUGGING
    if (debug & 2048)
	fprintf(stderr,"%s %d %d %d\n",s,tainted,uid, euid);
#endif
    if (tainted && (!euid || euid != uid)) {
	if (!unsafe)
	    fatal("%s", s);
	else if (dowarn)
	    warn("%s", s);
    }
}

taintenv()
{
    register STR *envstr;

    envstr = hfetch(stab_hash(envstab),"PATH",4,FALSE);
    if (!envstr || envstr->str_tainted) {
	tainted = 1;
	taintproper("Insecure PATH");
    }
    envstr = hfetch(stab_hash(envstab),"IFS",3,FALSE);
    if (envstr && envstr->str_tainted) {
	tainted = 1;
	taintproper("Insecure IFS");
    }
}
#endif /* TAINT */
