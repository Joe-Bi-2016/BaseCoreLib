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
class myMsgHandlerObj : public MsgHandlerObj
{
	public:
        void operator()(const Message& msg, void* context)
        { 
			void* p = msg->getParam();
			stringstream s;
			s << "what = " << msg->mWhat << " param = " << (p ? (char*)p : "nullptr") << endl;
			LOGD("[myMsgHandlerObj]:%s", s.str().c_str());
		}
};

void msgHandlerFun(const Message& msg, void* context)
{
	void *p = msg->getParam();
	stringstream s;
	s << "what = " << msg->mWhat << " param = " << (p ? (char *)p : "nullptr") << endl;
	LOGI("[msgHandlerFun]:%s", s.str().c_str());
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
	int exitWhat, exitModel, sleepMicroseconds = -1, msgPoolSize = 50;
	LOGI("%s", "**************************************************************");
	LOGI("%s", "* Start test main thread send message to looper of subthrad *");
	LOGI("%s", "**************************************************************");
	LOGI("[mainThread]-%s", "Enter the number of messages you want test:->");
	cin >> exitWhat;
	LOGI("[mainThread]-%s", "Enter quit model that you want test(0: general quit 1: safy quit):->");
	cin >> exitModel;
	LOGI("[mainThread]-%s", "Enter the interval between message sending(microseconds >= 0):->");
	while (sleepMicroseconds < 0)
	{
		cin >> sleepMicroseconds;
		if(sleepMicroseconds < 0)
			LOGI("[mainThread]-%s", "Please enter an integer greater than or equal to 0 :->");
	}
	LOGI("[mainThread]-%s", "Enter the messge pool size:->");
	cin >> msgPoolSize;

	LooperThread* looperThread = new LooperThread("SubLooperThread", msgPoolSize);
	Looper loop = looperThread->getLooper();	

	Handler h = MsgHandler::createHandler(loop);
	h->setMsgHandlerFunc(msgHandlerFun);

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

		Message msg = Msg::obtain(i, h);
		msg->setParam(strparam, strlen(strparam) + 1, freeMem);

		h->sendMessage(std::move(msg));

#if (defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__))
		std::this_thread::sleep_for(std::chrono::microseconds(sleepMicroseconds));
#else
		usleep(sleepMicroseconds);
#endif
		i++;
	}

	loop->getMsgQueue()->dumpQueueList();
	loop->getMsgQueue()->dumpQueuePool();

	delete looperThread;
	looperThread = nullptr;

	LOGI("SubLooper shared cnt = %ld\n", loop.use_count());

	//////////////////////////////////////////////////////////////////////////////
	// test looper in main thread, subthread send message
	LOGI("%s", "********************************************************");
	LOGI("%s", "* Start test subthread send message to looper of main *");
	LOGI("%s", "********************************************************");
	LOGI("[SubThread]-%s", "Enter the number of messages you want test:->");
	std::cin >> exitWhat;
	LOGI("[SubThread]-%s", "Enter quit model that you want test(0: general quit 1: safy quit):->");
	cin >> exitModel;
	LOGI("[SubThread]-%s", "Enter the interval between message sending(microseconds >= 0):->");
	sleepMicroseconds = -1;
	while (sleepMicroseconds < 0)
	{
		cin >> sleepMicroseconds;
		if (sleepMicroseconds < 0)
			LOGI("[SubThread]-%s", "Please enter an integer greater than or equal to 0 : ->");
	}
	LOGI("[SubThread]-%s", "Enter the messge pool size:->");
	cin >> msgPoolSize;

	looperThread = new LooperThread("MainLooperThread", msgPoolSize, true);
	Looper mainLoop = looperThread->getLooper();
	Handler mainH = MsgHandler::createHandler(mainLoop);
	myMsgHandlerObj msgHandlerObj;
	mainH->setMsgHandlerFunc(msgHandlerObj);
	std::thread th([&](void) {
		const char* ptr = "test subthread send message ";
		
		int i = 0;
		for (; ; )
		{
			if (i == exitWhat)
			{
				exitModel == 0 ? mainLoop->quit() : mainLoop->quit(true);
				break;
			}

			char* strparam = (char*)malloc(100 * sizeof(char));
			memset(strparam, 0x0, sizeof(char) * 100);
			snprintf(strparam, 100, "%s%d", ptr, i);

			Message msg = Msg::obtain(i, mainH);
			msg->setParam(strparam, strlen(strparam) + 1, freeMem);

			mainH->sendMessage(std::move(msg));

#if (defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__))
			std::this_thread::sleep_for(std::chrono::microseconds(sleepMicroseconds));
#else
			usleep(sleepMicroseconds);
#endif
			i++;
		}
		});
	th.detach();
	
	mainLoop->loop();

	mainLoop->getMsgQueue()->dumpQueueList();
	mainLoop->getMsgQueue()->dumpQueuePool();

	delete looperThread;
	looperThread = nullptr;

	LOGI("mainLoop shared cnt = %ld", mainLoop.use_count());

	system("pause");

    return 0;
}
