OBJS_LIBVSPACE := vspace.o

ALL_OBJS_LIBVSPACE := $(addprefix libvspace/, ${OBJS_LIBVSPACE})

${ALL_OBJS_LIBUTIL}: |libvspace

libvspace.a: ${ALL_OBJS_LIBVSPACE}
	${AR} crv $@ $^
	${RANLIB} $@

libvspace/%.o: ${LIBVSPACE_DIR}/%.c
	${CC} ${CFLAGS} -c -o $@ $<

libvspace:
	mkdir -p $@

clean::
	${RM} -f ${ALL_OBJS_LIBVSPACE} ${ALL_OBJS_LIBVSPACE:.o=.d}

clobber:: clean
	${RM} -f libvspace.a
	rmdir libvspace

-include ${ALL_OBJS_LIBVSPACE:.o=.d}