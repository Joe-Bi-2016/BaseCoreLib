#include "inc/os/Logger.h"
#include "inc/base/TimeUtil.h"
#include "inc/thread/MessageHandler.h"
#include "inc/thread/LooperThread.h"
#include <iomanip>
#include <typeinfo>
#include <cstring>
#include <iostream>
#include <fstream>
#include <sstream>
#include <math.h>

#ifdef LOG_TAG
#undef LOG_TAG
#endif
#define LOG_TAG (MAIN):

using namespace Root::Core;
using namespace std;

///////////////////////////////////////////////////////////////////////////
class myMsgHandlerFn : public MsgHandlerFunc
{
	public:
        void operator()(const Message& msg) 
        { 
			void* p = msg->getParam();
			stringstream s;
			s << "What = " << msg->mWhat << " Param = " << (p ? (char*)p : "NULL") << endl;
			LOGD("%s", s.str().c_str());
		}
};

int exitWhat = 10000;

void msgHandler(const Message& msg, void* context)
{
	void *p = msg->getParam();
	stringstream s;
	s << "What = " << msg->mWhat << " Param = " << (p ? (char *)p : "NULL") << endl;
	LOGI("%s", s.str().c_str());
}

void freeMem(void* obj, size_t bytes)
{
	if(obj)
	{
		free(obj);
		obj = nullptr;
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////
int main(void)
{	
	LOGI("%lld", utcMicrosTime2String(getNowTimeOfUs()));

	LooperThread* looperThread = new LooperThread("TestHandlerThread");
	Looper loop = looperThread->getLooper();

	LOGI("%s", "start to ....");

	Handler h = MsgHandler::createHandler(loop);
	h->setMsgHandlerFunc(msgHandler);

	const char* ptr = "test message ";

	int i = 0;
	for(; ; )
	{
		if(i == exitWhat)
		{
			looperThread->quitSafely();
			break;
		}

		char* strparam = (char*)malloc(100 * sizeof(char));
		memset(strparam, 0x0, sizeof(char) * 100);
		snprintf(strparam, 100, "%s%d", ptr, i);

		Message msg = std::move(Msg::obtain(i, h));
		msg->setParam(strparam, strlen(strparam) + 1, freeMem);

		h->sendMessage(std::move(msg));

		i++;
	}

	loop->getMsgQueue()->dumpQueueList();
	loop->getMsgQueue()->dumpQueuePool();

	delete looperThread;
	looperThread = NULL;

	LOGI("looper shared cnt = %ld", loop.use_count());

	system("pause");

    return 0;
}
