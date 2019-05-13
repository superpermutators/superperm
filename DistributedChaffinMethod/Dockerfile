FROM alpine

RUN apk add libc-dev curl
RUN apk add --no-cache --virtual .build-deps gcc make

COPY DistributedChaffinMethod.c /opt
COPY Makefile /opt
RUN ls /opt
RUN cd /opt && make

RUN apk del .build-deps

CMD ["/opt/DistributedChaffinMethod"]
