#include "inc/os/Logger.h"
#include "inc/base/TimeUtil.h"
#include "inc/looper/MessageHandler.h"
#include "inc/looper/LooperThread.h"
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
#define LOG_TAG (TestLooper):

using namespace Root::Core;
using namespace std;

/////////////////////////////////////////////////////////////////////////////////////////////////
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
	int exitWhat, exitModel, sleepMilliseconds = 0;
	LOGI("%s", "Enter the number of messages you want test:");
	cin >> exitWhat;
	LOGI("%s", "Enter quit model that you want test(0: general quit 1: safy quit):");
	cin >> exitModel;
	LOGI("%s", "Enter the interval between message sending(millisecond >= 0):");
	cin >> sleepMilliseconds;

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
			exitModel == 0 ? looperThread->quit() : looperThread->quitSafely();
			break;
		}

		char* strparam = (char*)malloc(100 * sizeof(char));
		memset(strparam, 0x0, sizeof(char) * 100);
		snprintf(strparam, 100, "%s%d", ptr, i);

		Message msg = std::move(Msg::obtain(i, h));
		msg->setParam(strparam, strlen(strparam) + 1, freeMem);

		h->sendMessage(std::move(msg));

#if (defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__))
		Sleep(sleepMilliseconds);
#else
		usleep((sleepMilliseconds / 1000));
#endif

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
