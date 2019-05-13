FROM alpine

# Build the application
RUN apk add libc-dev curl
RUN apk add --no-cache --virtual .build-deps gcc make
RUN mkdir /opt/dcm/
COPY DistributedChaffinMethod.c /opt/dcm/
COPY Makefile /opt/dcm/
RUN cd /opt/dcm && make
RUN apk del .build-deps


# Set up runtime for application
RUN mkdir /opt/workdir
RUN chmod ugo+rw /opt/workdir
VOLUME ["/opt/workdir/"]

# Set up command to actually run application
WORKDIR /opt/workdir/
CMD ["/opt/dcm/DistributedChaffinMethod"]
