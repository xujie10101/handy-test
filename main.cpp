#include "handy/handy.h"
#include "internal.h"

using namespace std;
using namespace handy;


#define		MAX_CONNS_WORKER	0x100000

void ModifytxQueue()
{
	socket_t fdClient = socket(AF_INET, SOCK_DGRAM, 0);

	struct ifconf lxfig = { 0 };
	char szBuf[1024] = { 0 };
	lxfig.ifc_buf = szBuf;
	lxfig.ifc_len = 1024;

	if (ioctl(fdClient, SIOCGIFCONF, &lxfig)) {
		return;
	}

	ifreq* it = lxfig.ifc_req;
	const struct ifreq* const end = it +
		(lxfig.ifc_len / sizeof(struct ifreq));

	for (; it != end; ++it) {
		if (0 == strcmp(it->ifr_name, "lo")) {
			continue;
		}
		it->ifr_ifru.ifru_ivalue = 51200;
		ioctl(fdClient, SIOCSIFTXQLEN, it);
	}
	::close(fdClient);
}

void ModifySyslimit()
{
	struct rlimit rlim, rlim_new;
	if (getrlimit(RLIMIT_NOFILE, &rlim) == 0)
	{
		rlim_new.rlim_cur = rlim_new.rlim_max = MAX_CONNS_WORKER;
		if (setrlimit(RLIMIT_NOFILE, &rlim_new) != 0) {
			rlim_new.rlim_cur = rlim_new.rlim_max = rlim.rlim_max;
			setrlimit(RLIMIT_NOFILE, &rlim_new);
		}
	}

	if (getrlimit(RLIMIT_CORE, &rlim) == 0) {
		rlim_new.rlim_cur = rlim_new.rlim_max = RLIM_INFINITY;
		if (setrlimit(RLIMIT_CORE, &rlim_new) != 0) {
			rlim_new.rlim_cur = rlim_new.rlim_max = rlim.rlim_max;
			setrlimit(RLIMIT_CORE, &rlim_new);
		}
	}

	if (system("sysctl -w net.core.somaxconn=65536")) {
		return;
	}

	if (system("sysctl -w net.core.rmem_max=8388608")) {
		return;
	}

	if (system("sysctl -w net.core.wmem_max=8388608")) {
		return;
	}

	if (system("sysctl -w net.core.rmem_default=8388608")) {
		return;
	}

	if (system("sysctl -w net.core.wmem_default=8388608")) {
		return;
	}

	if (system("sysctl -w net.ipv4.udp_rmem_min=8388608")) {
		return;
	}

	if (system("sysctl -w net.ipv4.udp_wmem_min=8388608")) {
		return;
	}

	if (system("sysctl -w net.ipv4.udp_mem='2097152 4194304 8388608'")) {
		return;
	}

	if (system("sysctl -w kernel.core_pattern=./core-%e-%t 1>/dev/null 2>&1")) {
		return;
	}
	/*
	if (system("sysctl -w net.ipv4.tcp_rmem='2097152 4194304 8388608'")) {
		return;
	}

	if (system("sysctl -w net.ipv4.tcp_wmem='2097152 4194304 8388608'")) {
		return;
	}

	if (system("sysctl -w net.ipv4.tcp_mem='2097152 4194304 8388608'")) {
		return;
	}
	*/
	//system("sysctl -a|grep net.core|sort");
	if (system("sysctl -p")) {
		return;
	}
}

int main(int argc, const char *argv[]) {
    ModifytxQueue();
#ifndef mips
	ModifySyslimit();
#endif

    int threads = 4;
    unsigned int n = std::thread::hardware_concurrency();
    if(0 != n){
        threads = n;
    }
    printf("threads:%d\n",threads);
    int port = 18080;
    if (argc > 1) {
        port = atoi(argv[1]);
    }
    setloglevel("TRACE");
    MultiBase base(threads);
    HttpServer sample(&base);
    int r = sample.bind("", port);
    exitif(r, "bind failed %d %s", errno, strerror(errno));

    std::map<std::string, std::string> headers;
    headers["GET"]="/hello";
    headers["GET"]="/";
    headers["POST"]="/test";
    for (auto &hd : headers) {
        sample.onMessage(hd.first, hd.second, [](const HttpConnPtr &con) {
            if(0 == strncmp(con.getRequest().method.c_str(),"GET",sizeof("GET"))){
                string v = con.getRequest().version;
                string m = con.getRequest().method;
                string b = con.getRequest().getBody();
                string a = con.getRequest().getArg("/");
                string a1 = con.getRequest().getArg("key1");
                // string h = con.getRequest().getHeader();
                HttpResponse resp;
                resp.body = Slice("/");
                resp.body.append(":");
                resp.body.append(b);
                con.sendResponse(resp);
                if (v == "HTTP/1.0") {
                    con->close();
                }
            }else if(0 == strncmp(con.getRequest().method.c_str(),"POST",sizeof("POST"))){
                string v = con.getRequest().version;
                string m = con.getRequest().method;
                string b = con.getRequest().getBody();
                HttpResponse resp;
                resp.body = Slice(b);
                con.sendResponse(resp);
                if (v == "HTTP/1.0") {
                    con->close();
                }
            }else if(0 == strncmp(con.getRequest().method.c_str(),"OPTIONS",sizeof("OPTIONS"))){
                string v = con.getRequest().version;
                string m = con.getRequest().method;
                string b = con.getRequest().getBody();
                HttpResponse resp;
                resp.body = Slice("");
                con.sendResponse(resp);
                if (v == "HTTP/1.0") {
                    con->close();
                }
            }else{
                string v = con.getRequest().version;
                string m = con.getRequest().method;
                HttpResponse resp;
                resp.body = Slice("NULL");
                con.sendResponse(resp);
                if (v == "HTTP/1.0") {
                    con->close();
                }
            }
        });
    }

    // // sample.onGet("/hello", [](const HttpConnPtr &con) {
    // sample.onMessage("GET", "/hello", [](const HttpConnPtr &con) {
    //     string m = con.getRequest().method;
    //     string v = con.getRequest().version;
    //     HttpResponse resp;
    //     resp.body = Slice("hello world");
    //     con.sendResponse(resp);
    //     if (v == "HTTP/1.0") {
    //         con->close();
    //     }
    // });
    // // sample.onGet("/", [](const HttpConnPtr &con) {
    // sample.onMessage("GET", "/", [](const HttpConnPtr &con) {
    //     string v = con.getRequest().version;
    //     string b = con.getRequest().getBody();
    //     string a = con.getRequest().getArg("/");
    //     string a1 = con.getRequest().getArg("key1");
    //     // string h = con.getRequest().getHeader();
    //     HttpResponse resp;
    //     resp.body = Slice("/");
    //     resp.body.append(":");
    //     resp.body.append(b);
    //     con.sendResponse(resp);
    //     if (v == "HTTP/1.0") {
    //         con->close();
    //     }
    // });
    // sample.onMessage("POST", "/test", [](const HttpConnPtr &con) {
    // string v = con.getRequest().version;
    // string b = con.getRequest().getBody();
    // HttpResponse resp;
    // resp.body = Slice(b);
    // con.sendResponse(resp);
    // if (v == "HTTP/1.0") {
    //     con->close();
    // }
    // });
    Signal::signal(SIGINT, [&] { base.exit(); });
    base.loop();
    return 0;
}
