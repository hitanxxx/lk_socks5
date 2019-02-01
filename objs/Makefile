
CC =	gcc
CFLAGS =  -fomit-frame-pointer -O -W -Wall -Wpointer-arith -Wconversion -Winline -Wmissing-prototypes -Wno-unused -Werror -g
CPP =	
LINK =	$(CC)


ALL_INCS = -I src \
	-I src/base \
	-I src/test \
	-I src/util \
	-I src/http \
	-I src/lktp \
	-I src/app \
	-I src/apis \
	-I objs


DEPS = src/lk_global.h \
	src/lk.h \
	src/base/l_mem.h \
	src/base/l_shm.h \
	src/base/l_config.h \
	src/base/l_time.h \
	src/base/l_string.h \
	src/base/l_meta.h \
	src/base/l_log.h \
	src/base/l_net.h \
	src/base/l_net_transport.h \
	src/base/l_event.h \
	src/base/l_listen.h \
	src/base/l_test.h \
	src/base/l_process.h \
	src/base/l_signal.h \
	src/base/l_json.h \
	src/base/l_timer.h \
	src/base/l_ssl.h \
	src/base/l_send.h \
	src/base/l_serv.h \
	src/http/l_http_entitybody.h \
	src/http/l_http_response_head.h \
	src/http/l_http_request_head.h \
	src/lktp/l_lktp_head.h \
	src/lktp/l_lktp_body.h \
	src/lktp/l_lktp_request.h \
	src/app/l_socks5_server.h \
	src/app/l_socks5_local.h \
	src/util/l_list.h \
	src/util/l_queue.h \
	src/util/l_bst.h \
	src/util/l_bheap.h

INCS = -I src \
	-I src/base \
	-I src/test \
	-I src/util \
	-I src/http \
	-I src/lktp \
	-I src/app \
	-I src/apis \
	-I objs


objs/lk:	objs/src/lk.o \
	objs/src/base/l_mem.o \
	objs/src/base/l_shm.o \
	objs/src/base/l_time.o \
	objs/src/base/l_string.o \
	objs/src/base/l_meta.o \
	objs/src/base/l_config.o \
	objs/src/base/l_event.o \
	objs/src/base/l_log.o \
	objs/src/util/l_list.o \
	objs/src/util/l_queue.o \
	objs/src/util/l_bst.o \
	objs/src/util/l_bheap.o \
	objs/src/base/l_net.o \
	objs/src/base/l_net_transport.o \
	objs/src/base/l_listen.o \
	objs/src/base/l_test.o \
	objs/src/base/l_timer.o \
	objs/src/base/l_process.o \
	objs/src/base/l_signal.o \
	objs/src/base/l_json.o \
	objs/src/base/l_ssl.o \
	objs/src/base/l_send.o \
	objs/src/base/l_serv.o \
	objs/src/http/l_http_entitybody.o \
	objs/src/http/l_http_response_head.o \
	objs/src/http/l_http_request_head.o \
	objs/src/lktp/l_lktp_head.o \
	objs/src/lktp/l_lktp_body.o \
	objs/src/lktp/l_lktp_request.o \
	objs/src/app/l_socks5_server.o \
	objs/src/app/l_socks5_local.o \
	objs/src/test/l_ts_mem.o \
	objs/src/test/l_ts_list.o \
	objs/src/test/l_ts_queue.o \
	objs/src/test/l_ts_bst.o \
	objs/src/test/l_ts_bheap.o \
	objs/src/test/l_ts_str.o \
	objs/src/test/l_ts_json.o
	$(LINK) -o objs/lk \
	objs/src/lk.o \
	objs/src/base/l_mem.o \
	objs/src/base/l_shm.o \
	objs/src/base/l_time.o \
	objs/src/base/l_string.o \
	objs/src/base/l_meta.o \
	objs/src/base/l_config.o \
	objs/src/base/l_event.o \
	objs/src/base/l_log.o \
	objs/src/util/l_list.o \
	objs/src/util/l_queue.o \
	objs/src/util/l_bst.o \
	objs/src/util/l_bheap.o \
	objs/src/base/l_net.o \
	objs/src/base/l_net_transport.o \
	objs/src/base/l_listen.o \
	objs/src/base/l_test.o \
	objs/src/base/l_timer.o \
	objs/src/base/l_process.o \
	objs/src/base/l_signal.o \
	objs/src/base/l_json.o \
	objs/src/base/l_ssl.o \
	objs/src/base/l_send.o \
	objs/src/base/l_serv.o \
	objs/src/http/l_http_entitybody.o \
	objs/src/http/l_http_response_head.o \
	objs/src/http/l_http_request_head.o \
	objs/src/lktp/l_lktp_head.o \
	objs/src/lktp/l_lktp_body.o \
	objs/src/lktp/l_lktp_request.o \
	objs/src/app/l_socks5_server.o \
	objs/src/app/l_socks5_local.o \
	objs/src/test/l_ts_mem.o \
	objs/src/test/l_ts_list.o \
	objs/src/test/l_ts_queue.o \
	objs/src/test/l_ts_bst.o \
	objs/src/test/l_ts_bheap.o \
	objs/src/test/l_ts_str.o \
	objs/src/test/l_ts_json.o \
	-lssl -lcrypto -pthread
	


objs/src/lk.o:	$(DEPS) \
	src/lk.c
	$(CC) -c $(CFLAGS) $(INCS) \
		-o objs/src/lk.o \
		src/lk.c


objs/src/base/l_mem.o:	$(DEPS) \
	src/base/l_mem.c
	$(CC) -c $(CFLAGS) $(INCS) \
		-o objs/src/base/l_mem.o \
		src/base/l_mem.c


objs/src/base/l_shm.o:	$(DEPS) \
	src/base/l_shm.c
	$(CC) -c $(CFLAGS) $(INCS) \
		-o objs/src/base/l_shm.o \
		src/base/l_shm.c


objs/src/base/l_time.o:	$(DEPS) \
	src/base/l_time.c
	$(CC) -c $(CFLAGS) $(INCS) \
		-o objs/src/base/l_time.o \
		src/base/l_time.c


objs/src/base/l_string.o:	$(DEPS) \
	src/base/l_string.c
	$(CC) -c $(CFLAGS) $(INCS) \
		-o objs/src/base/l_string.o \
		src/base/l_string.c


objs/src/base/l_meta.o:	$(DEPS) \
	src/base/l_meta.c
	$(CC) -c $(CFLAGS) $(INCS) \
		-o objs/src/base/l_meta.o \
		src/base/l_meta.c


objs/src/base/l_config.o:	$(DEPS) \
	src/base/l_config.c
	$(CC) -c $(CFLAGS) $(INCS) \
		-o objs/src/base/l_config.o \
		src/base/l_config.c


objs/src/base/l_event.o:	$(DEPS) \
	src/base/l_event.c
	$(CC) -c $(CFLAGS) $(INCS) \
		-o objs/src/base/l_event.o \
		src/base/l_event.c


objs/src/base/l_log.o:	$(DEPS) \
	src/base/l_log.c
	$(CC) -c $(CFLAGS) $(INCS) \
		-o objs/src/base/l_log.o \
		src/base/l_log.c


objs/src/util/l_list.o:	$(DEPS) \
	src/util/l_list.c
	$(CC) -c $(CFLAGS) $(INCS) \
		-o objs/src/util/l_list.o \
		src/util/l_list.c


objs/src/util/l_queue.o:	$(DEPS) \
	src/util/l_queue.c
	$(CC) -c $(CFLAGS) $(INCS) \
		-o objs/src/util/l_queue.o \
		src/util/l_queue.c


objs/src/util/l_bst.o:	$(DEPS) \
	src/util/l_bst.c
	$(CC) -c $(CFLAGS) $(INCS) \
		-o objs/src/util/l_bst.o \
		src/util/l_bst.c


objs/src/util/l_bheap.o:	$(DEPS) \
	src/util/l_bheap.c
	$(CC) -c $(CFLAGS) $(INCS) \
		-o objs/src/util/l_bheap.o \
		src/util/l_bheap.c


objs/src/base/l_net.o:	$(DEPS) \
	src/base/l_net.c
	$(CC) -c $(CFLAGS) $(INCS) \
		-o objs/src/base/l_net.o \
		src/base/l_net.c


objs/src/base/l_net_transport.o:	$(DEPS) \
	src/base/l_net_transport.c
	$(CC) -c $(CFLAGS) $(INCS) \
		-o objs/src/base/l_net_transport.o \
		src/base/l_net_transport.c


objs/src/base/l_listen.o:	$(DEPS) \
	src/base/l_listen.c
	$(CC) -c $(CFLAGS) $(INCS) \
		-o objs/src/base/l_listen.o \
		src/base/l_listen.c


objs/src/base/l_test.o:	$(DEPS) \
	src/base/l_test.c
	$(CC) -c $(CFLAGS) $(INCS) \
		-o objs/src/base/l_test.o \
		src/base/l_test.c


objs/src/base/l_timer.o:	$(DEPS) \
	src/base/l_timer.c
	$(CC) -c $(CFLAGS) $(INCS) \
		-o objs/src/base/l_timer.o \
		src/base/l_timer.c


objs/src/base/l_process.o:	$(DEPS) \
	src/base/l_process.c
	$(CC) -c $(CFLAGS) $(INCS) \
		-o objs/src/base/l_process.o \
		src/base/l_process.c


objs/src/base/l_signal.o:	$(DEPS) \
	src/base/l_signal.c
	$(CC) -c $(CFLAGS) $(INCS) \
		-o objs/src/base/l_signal.o \
		src/base/l_signal.c


objs/src/base/l_json.o:	$(DEPS) \
	src/base/l_json.c
	$(CC) -c $(CFLAGS) $(INCS) \
		-o objs/src/base/l_json.o \
		src/base/l_json.c


objs/src/base/l_ssl.o:	$(DEPS) \
	src/base/l_ssl.c
	$(CC) -c $(CFLAGS) $(INCS) \
		-o objs/src/base/l_ssl.o \
		src/base/l_ssl.c


objs/src/base/l_send.o:	$(DEPS) \
	src/base/l_send.c
	$(CC) -c $(CFLAGS) $(INCS) \
		-o objs/src/base/l_send.o \
		src/base/l_send.c


objs/src/base/l_serv.o:	$(DEPS) \
	src/base/l_serv.c
	$(CC) -c $(CFLAGS) $(INCS) \
		-o objs/src/base/l_serv.o \
		src/base/l_serv.c


objs/src/http/l_http_entitybody.o:	$(DEPS) \
	src/http/l_http_entitybody.c
	$(CC) -c $(CFLAGS) $(INCS) \
		-o objs/src/http/l_http_entitybody.o \
		src/http/l_http_entitybody.c


objs/src/http/l_http_response_head.o:	$(DEPS) \
	src/http/l_http_response_head.c
	$(CC) -c $(CFLAGS) $(INCS) \
		-o objs/src/http/l_http_response_head.o \
		src/http/l_http_response_head.c


objs/src/http/l_http_request_head.o:	$(DEPS) \
	src/http/l_http_request_head.c
	$(CC) -c $(CFLAGS) $(INCS) \
		-o objs/src/http/l_http_request_head.o \
		src/http/l_http_request_head.c


objs/src/lktp/l_lktp_head.o:	$(DEPS) \
	src/lktp/l_lktp_head.c
	$(CC) -c $(CFLAGS) $(INCS) \
		-o objs/src/lktp/l_lktp_head.o \
		src/lktp/l_lktp_head.c


objs/src/lktp/l_lktp_body.o:	$(DEPS) \
	src/lktp/l_lktp_body.c
	$(CC) -c $(CFLAGS) $(INCS) \
		-o objs/src/lktp/l_lktp_body.o \
		src/lktp/l_lktp_body.c


objs/src/lktp/l_lktp_request.o:	$(DEPS) \
	src/lktp/l_lktp_request.c
	$(CC) -c $(CFLAGS) $(INCS) \
		-o objs/src/lktp/l_lktp_request.o \
		src/lktp/l_lktp_request.c


objs/src/app/l_socks5_server.o:	$(DEPS) \
	src/app/l_socks5_server.c
	$(CC) -c $(CFLAGS) $(INCS) \
		-o objs/src/app/l_socks5_server.o \
		src/app/l_socks5_server.c


objs/src/app/l_socks5_local.o:	$(DEPS) \
	src/app/l_socks5_local.c
	$(CC) -c $(CFLAGS) $(INCS) \
		-o objs/src/app/l_socks5_local.o \
		src/app/l_socks5_local.c


objs/src/test/l_ts_mem.o:	$(DEPS) \
	src/test/l_ts_mem.c
	$(CC) -c $(CFLAGS) $(INCS) \
		-o objs/src/test/l_ts_mem.o \
		src/test/l_ts_mem.c


objs/src/test/l_ts_list.o:	$(DEPS) \
	src/test/l_ts_list.c
	$(CC) -c $(CFLAGS) $(INCS) \
		-o objs/src/test/l_ts_list.o \
		src/test/l_ts_list.c


objs/src/test/l_ts_queue.o:	$(DEPS) \
	src/test/l_ts_queue.c
	$(CC) -c $(CFLAGS) $(INCS) \
		-o objs/src/test/l_ts_queue.o \
		src/test/l_ts_queue.c


objs/src/test/l_ts_bst.o:	$(DEPS) \
	src/test/l_ts_bst.c
	$(CC) -c $(CFLAGS) $(INCS) \
		-o objs/src/test/l_ts_bst.o \
		src/test/l_ts_bst.c


objs/src/test/l_ts_bheap.o:	$(DEPS) \
	src/test/l_ts_bheap.c
	$(CC) -c $(CFLAGS) $(INCS) \
		-o objs/src/test/l_ts_bheap.o \
		src/test/l_ts_bheap.c


objs/src/test/l_ts_str.o:	$(DEPS) \
	src/test/l_ts_str.c
	$(CC) -c $(CFLAGS) $(INCS) \
		-o objs/src/test/l_ts_str.o \
		src/test/l_ts_str.c


objs/src/test/l_ts_json.o:	$(DEPS) \
	src/test/l_ts_json.c
	$(CC) -c $(CFLAGS) $(INCS) \
		-o objs/src/test/l_ts_json.o \
		src/test/l_ts_json.c


install:
	test -d /usr/local/lk || mkdir -p /usr/local/lk
	test -d /usr/local/lk/config || cp -rf config /usr/local/lk/config
	test -d /usr/local/lk/log || mkdir -p /usr/local/lk/logs
	test -d /usr/local/lk/www || cp -rf www /usr/local/lk
	test -d /usr/local/lk/sbin || mkdir -p /usr/local/lk/sbin
	cp objs/lk /usr/local/lk/sbin/
