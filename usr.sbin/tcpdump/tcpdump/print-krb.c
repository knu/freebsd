/*
 * Copyright (c) 1995
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that: (1) source code distributions
 * retain the above copyright notice and this paragraph in its entirety, (2)
 * distributions including binary code include the above copyright notice and
 * this paragraph in its entirety in the documentation or other materials
 * provided with the distribution, and (3) all advertising materials mentioning
 * features or use of this software display the following acknowledgement:
 * ``This product includes software developed by the University of California,
 * Lawrence Berkeley Laboratory and its contributors.'' Neither the name of
 * the University nor the names of its contributors may be used to endorse
 * or promote products derived from this software without specific prior
 * written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 * Initial contribution from John Hawkinson (jhawk@mit.edu).
 */

#ifndef lint
static char rcsid[] =
  "$FreeBSD$";
/*
    "@(#) Header: /afs/sipb/project/tcpdump/src/tcpdump-3.0.2/RCS/print-krb.c,v 1.3 1995/08/16 05:33:27 jhawk Exp ";
*/
#endif

#include <sys/param.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>
#include <netinet/udp.h>
#include <netinet/udp_var.h>

#include <ctype.h>
#include <errno.h>
#include <stdio.h>

#include "interface.h"
#include "addrtoname.h"

const char *c_print(register const u_char *s, register const u_char *ep);
const u_char *krb4_print_hdr(const u_char *cp);
void krb4_print(const u_char *cp);
void krb_print(const u_char *dat, int length);


#define         AUTH_MSG_KDC_REQUEST                     1<<1
#define         AUTH_MSG_KDC_REPLY                       2<<1
#define         AUTH_MSG_APPL_REQUEST                    3<<1
#define         AUTH_MSG_APPL_REQUEST_MUTUAL             4<<1
#define         AUTH_MSG_ERR_REPLY                       5<<1
#define         AUTH_MSG_PRIVATE                         6<<1
#define         AUTH_MSG_SAFE                            7<<1
#define         AUTH_MSG_APPL_ERR                        8<<1
#define         AUTH_MSG_DIE                            63<<1

#define         KERB_ERR_OK                              0
#define         KERB_ERR_NAME_EXP                        1
#define         KERB_ERR_SERVICE_EXP                     2
#define         KERB_ERR_AUTH_EXP                        3
#define         KERB_ERR_PKT_VER                         4
#define         KERB_ERR_NAME_MAST_KEY_VER               5
#define         KERB_ERR_SERV_MAST_KEY_VER               6
#define         KERB_ERR_BYTE_ORDER                      7
#define         KERB_ERR_PRINCIPAL_UNKNOWN               8
#define         KERB_ERR_PRINCIPAL_NOT_UNIQUE            9
#define         KERB_ERR_NULL_KEY                       10

struct krb {
  u_char pvno;		/* Protocol Version */
  u_char type;		/* Type+B */
} ;

static char tstr[] = " [|kerberos]";

static struct token type2str[] = { 
    { AUTH_MSG_KDC_REQUEST,		"KDC_REQUEST" },
    { AUTH_MSG_KDC_REPLY,		"KDC_REPLY" },
    { AUTH_MSG_APPL_REQUEST,		"APPL_REQUEST" },
    { AUTH_MSG_APPL_REQUEST_MUTUAL,	"APPL_REQUEST_MUTUAL" },
    { AUTH_MSG_ERR_REPLY,		"ERR_REPLY" },
    { AUTH_MSG_PRIVATE,			"PRIVATE" },
    { AUTH_MSG_SAFE,			"SAFE" },
    { AUTH_MSG_APPL_ERR,		"APPL_ERR" },
    { AUTH_MSG_DIE,			"DIE" },
    { 0,				NULL }
};

static struct token kerr2str[] = {
    { KERB_ERR_OK,                 	"OK" },
    { KERB_ERR_NAME_EXP,           	"NAME_EXP" },
    { KERB_ERR_SERVICE_EXP,    	   	"SERVICE_EXP" },
    { KERB_ERR_AUTH_EXP,           	"AUTH_EXP" },   
    { KERB_ERR_PKT_VER,            	"PKT_VER" }, 
    { KERB_ERR_NAME_MAST_KEY_VER,   	"NAME_MAST_KEY_VER" },
    { KERB_ERR_SERV_MAST_KEY_VER,  	"SERV_MAST_KEY_VER" },
    { KERB_ERR_BYTE_ORDER,         	"BYTE_ORDER" },       
    { KERB_ERR_PRINCIPAL_UNKNOWN,  	"PRINCIPAL_UNKNOWN" },
    { KERB_ERR_PRINCIPAL_NOT_UNIQUE,	"PRINCIPAL_NOT_UNIQUE" },
    { KERB_ERR_NULL_KEY,            	"NULL_KEY"},    	 
    { 0,				NULL}
  };


/* little endian (unaligned) to host byte order */
#define vtohlp(x)	    ((( ((char*)(x))[0] )      )  | \
			     (( ((char*)(x))[1] ) <<  8)  | \
			     (( ((char*)(x))[2] ) << 16)  | \
			     (( ((char*)(x))[3] ) << 24))
#define vtohsp(x)          ((( ((char*)(x))[0] )      )  | \
			     (( ((char*)(x))[1] ) <<  8))
/* network (big endian) (unaligned) to host byte order */
#define ntohlp(x)	    ((( ((char*)(x))[3] )      )  | \
			     (( ((char*)(x))[2] ) <<  8)  | \
			     (( ((char*)(x))[1] ) << 16)  | \
			     (( ((char*)(x))[0] ) << 24))
#define ntohsp(x)          ((( ((char*)(x))[1] )      )  | \
			     (( ((char*)(x))[0] ) <<  8))



const char *
c_print(register const u_char *s, register const u_char *ep)
{
	register u_char c;
	register int flag;

	flag = 1;
	while (ep == NULL || s < ep) {
		c = *s++;
		if (c == '\0') {
			flag = 0;
			break;
		}
		if (!isascii(c)) {
			c = toascii(c);
			putchar('M');
			putchar('-');
		}
		if (!isprint(c)) {
			c ^= 0x40;	/* DEL to ?, others to alpha */
			putchar('^');
		}
		putchar(c);
	}
	if (flag)
	  return NULL;
	return(s);
}


const u_char *
krb4_print_hdr(const u_char *cp)
{
	  cp+=2;

#define TCHECK		if (cp >= snapend)  goto trunc
#define PRINT		if ((cp=c_print(cp, snapend))==NULL) goto trunc

	  TCHECK;
	  PRINT;
	  TCHECK;
	  putchar('.'); PRINT;
	  TCHECK;
	  putchar('@'); PRINT;
	  return(cp);

	  trunc:
	    fputs(tstr, stdout);
	    return(NULL);

#undef TCHECK
#undef PRINT
}

void krb4_print(const u_char *cp)
{
	register const struct krb *kp;
	u_char type;
	u_short len;

#define TCHECK		if (cp >= snapend) goto trunc
#define PRINT		if ((cp=c_print(cp, snapend))==NULL) goto trunc
#define ENDIAN		(kp->type & 0x01)
/* ENDIAN is 1 for little, 0 for big */

	kp = (struct krb *)cp;

	if ((&kp->type) >= snapend) {
		fputs(tstr, stdout);
		return;
	}

	type = kp->type & (0xFF << 1);

	printf(" %s %s: ", ENDIAN?"le":"be", tok2str(type2str, NULL, type));

	switch(type) {
	case AUTH_MSG_KDC_REQUEST:
	  if ((cp=krb4_print_hdr(cp)) == NULL)
	    return;
	  cp+=4; 	  /* ctime */
	  TCHECK;
	  printf(" %dmin ", *cp++ * 5);
	  TCHECK;
	  PRINT;
	  TCHECK;
	  putchar('.');  PRINT;
	  break;
	case AUTH_MSG_APPL_REQUEST:
	  cp+=2;
	  TCHECK;
	  printf("v%d ", *cp++);
	  TCHECK;
	  PRINT;
	  TCHECK;
	  printf(" (%d)", *cp++);
	  TCHECK;
	  printf(" (%d)", *cp);
	  TCHECK;
	  break;
	case AUTH_MSG_KDC_REPLY:
	  if ((cp=krb4_print_hdr(cp)) == NULL)
	    return;
	  cp+=10; /* timestamp + n + exp + kvno */
	  TCHECK;
	  len=ENDIAN? vtohsp(cp) : ntohsp(cp);
	  printf(" (%d)", len);
	  TCHECK;
	  break;
	case AUTH_MSG_ERR_REPLY:
	  if ((cp=krb4_print_hdr(cp)) == NULL)
	    return;
	  cp+=4; 	  /* timestamp */
	  TCHECK;
	  printf(" %s ", tok2str(kerr2str, NULL,
				 ENDIAN? vtohlp(cp) :
				         ntohlp(cp)
				 ));
	  cp+=4;
	  TCHECK;
	  PRINT;
	  break;
	default:
	  fputs("(unknown)", stdout);
	  break;
	}

	return;
trunc:
	fputs(tstr, stdout);
#undef TCHECK
}

void
krb_print(const u_char *dat, int length)
{
	register const struct krb *kp;

	kp = (struct krb *)dat;

	if (dat >= snapend) {
	  fputs(tstr, stdout);
	  return;
	}

	switch (kp->pvno) {
	case 1:
	case 2:
	case 3:
	  printf(" v%d", kp->pvno);
	  break;
	case 4:
	  printf(" v%d", kp->pvno);
	  krb4_print((const u_char*)kp);
	  break;
	case 106:
	case 107:
	  fputs(" v5", stdout);
	  /* Decode ASN.1 here "someday" */
	  break;
	}
	return;
}
