#include "handy/handy.h"
#include "internal.h"
#include "Data.h"
#include "log4z.h"
#include "cJSON.h"


using namespace std;
using namespace handy;

static std::map<CData, CData> mMapFile;


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

int GetFilePaths(const char* directory)
{
	DIR* dir = 0;
	struct stat st;
	char sFilePath[256] = {0};
	struct dirent* filename;
	if (0 == getcwd(sFilePath, 256)) {
		LOGFMTE("Get path fail!");
		return XX_ERR_NONE;
	}

	if (0 == directory) {
		LOGFMTE(" directory is null ! :%s", directory);
		return XX_ERR_ERROR;
	}
	lstat(directory, &st);
	if (!S_ISDIR(st.st_mode)) {
		LOGFMTE("directory is not a valid directory ! :%s", directory);
		return XX_ERR_ERROR;
	}

	if (0 == (dir = opendir(directory))) {
		LOGFMTE("Can not open dir %s", directory);
		return XX_ERR_ERROR;
	}
	/* read all the files in the dir ~ */
	while ((filename = readdir(dir)) != 0) {
		if (strcmp(filename->d_name, ".") == 0 ||
			strcmp(filename->d_name, "..") == 0) {
			continue;
		}
		snprintf(sFilePath, sizeof(sFilePath), "%s/%s", directory, filename->d_name);

		if (lstat(sFilePath, &st) == -1) {
			LOGFMTE("directory is not a valid directory ! %s",sFilePath);
			return XX_ERR_ERROR;
		}

		if (S_ISDIR(st.st_mode)) {
			GetFilePaths(sFilePath);
			continue;
		}
		snprintf(sFilePath, sizeof(sFilePath), "%s/%s", directory, filename->d_name);
		// CAutoMutex AutoMutex(m_mutexMapFile);
		mMapFile[filename->d_name] = sFilePath;
		LOGFMTI("[%s][%s]",filename->d_name,sFilePath);
	}
	return XX_ERR_NONE;
}

void sendJson(const HttpConnPtr &con, cJSON* json){
	char* ptr = cJSON_Print(json);
	// sendSessionMessage(ptr);
	HttpResponse resp;
	resp.body = Slice(ptr);
	con.sendResponse(resp);
	cJSON_Delete(json);
	SAFE_FREE(ptr);
}

void getMachineCode(const HttpConnPtr &con) {

#if 0
	m_local_uid = "6fd8a6b13ea6368130696af1ab494eb1";
	char szStr[1024] = { 0 };
#if  defined (__aarch64__) || defined(mips)
	printf("__aarch64__\n");
	std::string cmd = "cat /proc/cpuinfo |grep Serial | awk '{print $3}'";
	ExecuteCMD(cmd.c_str(), szStr);
	UID += szStr;
	cmd = "cat /sys/bus/mmc/devices/mmc1\\:0001/cid";
	ExecuteCMD(cmd.c_str(), szStr);
	UID += szStr;
	char szMd5[64] = { 0 };
	MD5Digest((char*)UID.c_str(), UID.size(), szMd5);
	UID = szMd5;
	LOGFMTI("szMd5:%s", szMd5);
#elif defined __x86_64__
	printf("__x86_64__\n");
	printf("%s\n",m_local_uid.c_str());
#endif
#endif

	cJSON* JsonObject1 = cJSON_CreateObject();
	cJSON_AddItemToObject(JsonObject1, "code", cJSON_CreateNumber(200));
	cJSON_AddItemToObject(JsonObject1, "msg", cJSON_CreateString("m_local_uid.c_str()"));
	cJSON_AddItemToObject(JsonObject1, "data", cJSON_CreateString("success"));

	sendJson(con, JsonObject1);
}

void getProjectInfo(const HttpConnPtr &con) {
	int nType = 1;
	std::string projectName = "";
	std::string code = "";
	std::string versionNum = "";
	if (0 == nType){
		projectName = "安全联网网关";
		versionNum = "AQLW-P-MD1000";
		code = "209";
	}else if (1 == nType){
//#ifndef mips
//		projectName = "安全加固网关";
//#else
		projectName = "视频安全接入网关";
//#endif
		versionNum = "ANCHOR_AQJR";
		code = "118";
	}else if (2 == nType){
		projectName = "前端转换设备";
		versionNum = "ZD8911 A3-4";
		code = "118";
	}else if (3 == nType) {
		projectName = "安全联网网关";
		versionNum = "ZXH";
		code = "209";
	}else if (4 == nType) {
		projectName = "安全加固网关";
		versionNum = "ZXH";
		code = "118";
	}else if (5 == nType) {
		projectName = "安全联网网关";
		versionNum = "AQLW-P-XDJAOA";
		code = "118";
	}

	cJSON* JsonObject1 = cJSON_CreateObject();
	cJSON* JsonObject2 = cJSON_CreateObject();
	cJSON_AddItemToObject(JsonObject1, "code", cJSON_CreateNumber(200));
	cJSON_AddItemToObject(JsonObject1, "msg", cJSON_CreateString("成功"));
	cJSON_AddItemToObject(JsonObject1, "data", JsonObject2);

	cJSON_AddItemToObject(JsonObject2, "code", cJSON_CreateString("200"));
	cJSON_AddItemToObject(JsonObject2, "versionNum", cJSON_CreateString("buildVersion.c_str()"));
	char st_time[64] = { 0 };

	cJSON_AddItemToObject(JsonObject2, "time", cJSON_CreateString("st_time"));
	cJSON_AddItemToObject(JsonObject2, "type", cJSON_CreateString(CData(nType).c_str()));
	cJSON_AddItemToObject(JsonObject2, "projectName", cJSON_CreateString(projectName.c_str()));

	sendJson(con,JsonObject1);
}

void preHandle(const HttpConnPtr &con) {
	cJSON* JsonObject1 = cJSON_CreateObject();
	cJSON* JsonObject2 = cJSON_CreateObject();

	#if LICENSE
		int num = licenseCheck(m_local_uid);
	#else
		int num = 0;
	#endif

	if (num != 0)
	{
		LOGFMTE("证书验证失败，%d", num);
		cJSON_AddItemToObject(JsonObject1, "code", cJSON_CreateNumber(400));
		cJSON_AddItemToObject(JsonObject1, "msg", cJSON_CreateString("证书验证失败"));
		cJSON_AddItemToObject(JsonObject1, "data", NULL);
	}
	else
	{
		cJSON_AddItemToObject(JsonObject1, "code", cJSON_CreateNumber(200));
		cJSON_AddItemToObject(JsonObject1, "msg", cJSON_CreateString("成功"));
		cJSON_AddItemToObject(JsonObject1, "data", JsonObject2);
		cJSON_AddItemToObject(JsonObject2, "machineCode", cJSON_CreateString("machineCode.c_str()"));
#if LICENSE
		cJSON_AddItemToObject(JsonObject2, "issuedTime", cJSON_CreateString(g_start_time.c_str()));//开始时间
		cJSON_AddItemToObject(JsonObject2, "expiryTime", cJSON_CreateString(g_end_time.c_str()));//结束时间
		int n1 = 0, m1 = 0, d1 = 0, n2 = 0, m2 = 0, d2 = 0;
		sscanf((char*)g_end_time.c_str(), "%d-%d-%d", &n2, &m2, &d2);
		sscanf((char*)g_start_time.c_str(), "%d-%d-%d", &n1, &m1, &d1);

		int sum = 0;
		//scanf("%d %d %d", &n1, &m1, &d1);//起始年月日 
		//scanf("%d %d %d", &n2, &m2, &d2);//最终年月日 
		sum = dateDifference(n1, m1, d1, n2, m2, d2);
		cJSON_AddItemToObject(JsonObject2, "days", cJSON_CreateNumber(sum));	//剩余时间
		std::string temp = "证书校验通过，证书有效期：";
		temp += g_start_time.c_str();
		temp += " - ";
		temp += g_end_time.c_str();
		cJSON_AddItemToObject(JsonObject2, "message", cJSON_CreateString(temp.c_str()));
#else
		cJSON_AddItemToObject(JsonObject2, "issuedTime", cJSON_CreateString(""));//开始时间
		cJSON_AddItemToObject(JsonObject2, "expiryTime", cJSON_CreateString(""));//结束时间
		cJSON_AddItemToObject(JsonObject2, "days", cJSON_CreateNumber(0));	//剩余时间
		cJSON_AddItemToObject(JsonObject2, "message", cJSON_CreateString(""));
#endif
	}
	sendJson(con, JsonObject1);
}

int main(int argc, const char *argv[]) {
    GetFilePaths("web");
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
    headers["/hello"]="GET";
    headers["/"]="GET";
    headers["/test"]="POST";
	headers["/license/getProjectInfo"]="POST";
	headers["/license/getMachineCode"]="POST";
	headers["/license/preHandle"]="POST";
	for (const auto& pair : mMapFile) {
		// pair.first 是map的键，pair.second是map的值
		// 在这里对pair进行操作
		headers[pair.second.ToString().substr(3)]="GET";
		LOGFMTA("----:%s",pair.first.c_str());
		LOGFMTA("====:%s",pair.second.ToString().substr(3).c_str());
	}

    for (auto &hd : headers) {
        sample.onMessage(hd.second, hd.first, [](const HttpConnPtr &con) {
            if(0 == strncmp(con.getRequest().method.c_str(),"GET",sizeof("GET"))){
                string v = con.getRequest().version;
                string m = con.getRequest().method;
                string b = con.getRequest().getBody();
                string a = con.getRequest().getArg("");
				CData sFileName = GetUrlPath(con.getRequest().uri.c_str(), "/");
				std::map<CData, CData>::iterator Iter;
				if (sFileName.empty() || sFileName == "/") { sFileName = "index.html";LOGFMTI("----------"); }
				LOGFMTI("%s", sFileName.c_str());
				if ((Iter = mMapFile.find(sFileName)) != mMapFile.end()) {
					// CAutoMutex AutoMutex(m_mutexMapFile);
					con.sendFile(Iter->second.c_str());
					// usleep(200);
				}
				else {
                    HttpResponse resp;
                    resp.setNotFound();
                    con.sendResponse(resp);
					LOGFMTE("%s", sFileName.c_str());
				}
				// con->close();
            }else if(0 == strncmp(con.getRequest().method.c_str(),"POST",sizeof("POST"))){
				CData ApiStr = con.getRequest().uri;
				if(ApiStr == "/license/getProjectInfo"){
					getProjectInfo(con);
				}else if (ApiStr == "/license/getMachineCode") {
					getMachineCode(con);
				}else if (ApiStr == "/license/preHandle") {
					preHandle(con);
				}
				con->close();
                // string v = con.getRequest().version;
                // string m = con.getRequest().method;
                // string b = con.getRequest().getBody();
                // HttpResponse resp;
                // resp.body = Slice(b);
                // con.sendResponse(resp);
                // if (v == "HTTP/1.0") {
                //     con->close();
                // }
            }else if(0 == strncmp(con.getRequest().method.c_str(),"OPTIONS",sizeof("OPTIONS"))){
                string v = con.getRequest().version;
                string m = con.getRequest().method;
                string b = con.getRequest().getBody();
                HttpResponse resp;
                resp.body = Slice("");
                con.sendResponse(resp);
                con->close();
            }else{
                string v = con.getRequest().version;
                string m = con.getRequest().method;
                HttpResponse resp;
                resp.body = Slice("NULL");
                con.sendResponse(resp);
                con->close();
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
