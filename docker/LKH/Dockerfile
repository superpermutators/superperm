from ubuntu:latest

RUN apt-get -y update
RUN apt-get -y install curl make gcc git python
RUN curl -LO http://www.akira.ruc.dk/~keld/research/LKH/LKH-2.0.7.tgz
RUN tar zxf LKH-2.0.7.tgz
RUN cd LKH-2.0.7 && make && cp ./LKH /usr/local/bin/

RUN git clone https://github.com/superpermutators/superperm.git superperm

RUN echo "cd superperm && /usr/bin/python /superperm/bin/lkh_runner.py -o /mnt/out" > run.sh
RUN chmod +x run.sh
