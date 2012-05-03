PROG=	supervise

DPADD=  ${LIBUTIL}
LDADD=  -lutil

BINDIR?=	/sbin

WARNS?=	100

NO_MAN=	yes

.include <bsd.prog.mk>
