# Base core Libarary
A library of basic C/C++ feature suites that support C++11 and POSIX standards as well as Windows, Linux, macOS, and Android platforms.It contain file operation, mutex, condition variable, semaphore, thread pool, circular queue, looper that similar with android, spdlog library for log.

## Note:
If want generate library, should change basecore/CMakeLists.txt's add_executable to target_link_libraries.

## window: 
use Cmake-GUI to generate vs project and build it.

## linux/android/macos:
mkdir build
cd build
cmake ..
cmake --build .

## Example for testing Looper:

```
void msgHandlerFun(const Message& msg, void* context){
	void *p = msg->getParam();
	stringstream s;
	s << "what = " << msg->mWhat << " param = " << (p ? (char *)p : "nullptr") << endl;
	LOGI("[msgHandlerFun]:%s", s.str().c_str());
}

void freeMem(void* obj, size_t bytes){
	if(obj){
		free(obj);
		obj = nullptr;
	}
}

class myMsgHandlerObj : public MsgHandlerObj
{
	public:
        void operator()(const Message& msg, void* context) { 
			void* p = msg->getParam();
			stringstream s;
			s << "what = " << msg->mWhat << " param = " << (p ? (char*)p : "nullptr") << endl;
			LOGD("[myMsgHandlerObj]:%s", s.str().c_str());
		}
};
```
###### >1 main thread send message to looper of subthread:
```
LooperThread* looperThread = new LooperThread("LooperThread", msgPoolSize);
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
```
###### >2 subthread send message to looper of main
```
looperThread = new LooperThread("LooperThread", msgPoolSize, true);
Looper mainLoop = MsgLooper::prepare(msgPoolSize);
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
```