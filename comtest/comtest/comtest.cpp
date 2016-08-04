// comtest.cpp : �������̨Ӧ�ó������ڵ㡣
//

#include "stdafx.h" 
#include <stdio.h>
#include <list>
#include <winsock2.h>
#include "MyMutex.h"

#pragma comment(lib, "ws2_32.lib")
//����ԭ��
void init_com(void);       //���崮�ڳ�ʼ������ 
int init_socket();
void char2hex(unsigned char*p, int len);
DWORD WINAPI RcvFromCom(LPVOID lParam); //�������̺߳���
DWORD WINAPI SendToCom(LPVOID lParam); //�������̺߳���

#define UART_LEN 256
#define MYCOM "com6"

//�ṹ����
struct Thread_Data        //����һ���ṹ���ͣ����ڴ����������߳� 
{ 
	HANDLE hcom; 
	OVERLAPPED vcc; 
};
struct UartData
{
	UartData(char* _p, int _len)
	{
		len = _len;
		memset(p, 0, UART_LEN);
		memcpy(p, _p, len);
	}
	char p[UART_LEN];
	int len;
};
std::list<UartData> g_SendMsgList;
std::list<UartData> g_ReceiveMsgList;

class RcvBuf
{
public:
	RcvBuf()
	{
		clear();
	}
	void writeInReceiveBuf(char* p, int len)
	{	
		//if (!checkData())
		//{
		//	printf("head error!");
		//}
		memcpy(buf+end, p, len); 
		end += len;
		if (checkBuf())
		{
			//sendtocom
			printf("receive data from com len: %d\n",  dataLen);
			char2hex(buf+start, dataLen);
			g_ReceiveMsgList.push_back(UartData((char*)buf+start, dataLen));
			printf("g_ReceiveMsgList.size[%d]\n", g_ReceiveMsgList.size());
			clear();
		}
		
	}
private:
	void clear()
	{
		memset(buf, 0, UART_LEN);
		start=0;
		end=0;
		dataLen=0;
	}
	void checkData();
	void findHead()
	{
		bool find=false;
		for (int i=0;i<end;i++)
		{
			if (buf[i] == 0xf4)
			{
				find=true;
				start=i;
				if (i+1<end && buf[i+1]==0xf5)
				{
					if (i+3<end)
					{
						dataLen=buf[i+2]*256+buf[i+3]+4;
						return;
					}
				}
			}
		}
		if (!find)
			clear();
	}
	bool checkBuf()
	{
		findHead();
		if (dataLen!=0)
		{
			if (dataLen <= end-start)
			{
				return true;
			}
		}
		return false;
	}
	void move()
	{
		end=end-dataLen;
		memcpy(buf, buf+dataLen, end);
		memset(buf+end, 0, UART_LEN-end);
		start=0;
		dataLen=0;
	}
	

	unsigned char buf[UART_LEN];
	int start;
	int end;
	int dataLen;
};
RcvBuf g_RcvBuf;


//��������
unsigned char receive_data[UART_LEN];     //����������ݻ��� 
HANDLE m_hCom = NULL;       //����һ����������ڴ򿪴��� 
CRITICAL_SECTION com_cs;      //�����ٽ��������
CRITICAL_SECTION com_cs2;      //�����ٽ��������

HANDLE mutexSend;
HANDLE mutexReceive;


//=======================================================================================
int main(int argc, char* argv[]) 
{ 
	printf("******************����̨���Թ���*********************\n");

	InitializeCriticalSection(&com_cs);   //��ʼ���ٽ����
	InitializeCriticalSection(&com_cs2);   //��ʼ���ٽ����
	init_com(); //��ʼ������ 
	init_socket();
	//while(1) 
	//{ 
	//	Sleep(200);//ʹ���̹߳��� 
	//}
	// CloseHandle(m_hCom);
	printf("\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n");
	printf("******************����̨���Թ���*********************\n");
	getchar();
	return 0; 
}
void char2hex(unsigned char* p, int len)
{
	for (int i=0;i<len;i++)
	{
		printf("%#x ", p[i]);
	}
	printf("\n");
}
int init_socket()
{
	WSADATA wsaData;
	WORD sockVersion = MAKEWORD(2,2);
	if(WSAStartup(sockVersion, &wsaData) != 0)
	{
		return 0;
	}

	SOCKET serSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP); 
	if(serSocket == INVALID_SOCKET)
	{
		printf("socket error !");
		return 0;
	}

	sockaddr_in serAddr;
	serAddr.sin_family = AF_INET;
	serAddr.sin_port = htons(8888);
	serAddr.sin_addr.S_un.S_addr = INADDR_ANY;
	if(bind(serSocket, (sockaddr *)&serAddr, sizeof(serAddr)) == SOCKET_ERROR)
	{
		printf("bind error !");
		closesocket(serSocket);
		return 0;
	}

	sockaddr_in remoteAddr;
	int nAddrLen = sizeof(remoteAddr); 
	while (true)
	{
		char recvData[UART_LEN];  
		int ret = recvfrom(serSocket, recvData, UART_LEN, 0, (sockaddr *)&remoteAddr, &nAddrLen);
		if (ret < 0)
		{
			continue;
		}

		//char *p = new char[UART_LEN];
		//memset(p, 0, UART_LEN);
		//memcpy(p, recvData, ret);
		if (ret>8)
		{
			//send to com
			g_SendMsgList.push_back(UartData(recvData,ret));
		}

		if (ret > 8)
		{
			recvData[ret] = 0x00;
			printf("reveive data from: %s len:%d\r\n", inet_ntoa(remoteAddr.sin_addr), ret);
			char2hex((unsigned char*)recvData, ret);            
		}
		static int aa=0;
		char sendData[UART_LEN]={0};
		sprintf_s(sendData, "my name is c udp server!%d\n", aa++ );
		Sleep(50);
		//sendto(serSocket, sendData, (int)strlen(sendData), 0, (sockaddr *)&remoteAddr, nAddrLen);
		if (g_ReceiveMsgList.size()>0)
		{
			UartData seData = g_ReceiveMsgList.front();
			char pData[UART_LEN]= {0};
			int datalen=seData.len;
			memcpy(pData, seData.p, seData.len);
			g_ReceiveMsgList.pop_front();
			printf("g_ReceiveMsgList.size[%d]-->[%d]\n", g_ReceiveMsgList.size()+1, g_ReceiveMsgList.size());
			sendto(serSocket, pData, datalen, 0, (sockaddr *)&remoteAddr, nAddrLen);
		}
		else
		{
			sendto(serSocket, "null", (int)strlen("null"), 0, (sockaddr *)&remoteAddr, nAddrLen);
		}
		
	}
	closesocket(serSocket); 
	WSACleanup();
	return 0;
}

void init_com() 
{
	EnterCriticalSection(&com_cs);//�����ٽ�� 
	HANDLE hThread1 = NULL;//���߳̾��
	HANDLE hThread2 = NULL;//���߳̾��
	COMMTIMEOUTS TimeOuts;//���峬ʱ�ṹ 
	DCB dcb; //���崮���豸���ƽṹ 
	OVERLAPPED wrOverlapped;//�����첽�ṹ
	OVERLAPPED wrOverlapped2;//�����첽�ṹ
	static struct Thread_Data thread_data;//����һ���ṹ�������ڸ����̴߳��ݲ���
	static struct Thread_Data thread_data2;//����һ���ṹ�������ڸ����̴߳��ݲ���

	//1. �򿪴����ļ�
	printf("iput com number: \n");
	char chr[64]={0};
	gets_s(chr, 64);
	int com=atoi(chr);
	if (com<1||com>100)
	{
		printf("warning! com[%d] is error!\n", com);
	}
	sprintf_s(chr, 64, "\\\\.\\com%d", com);
	//printf("%d\n", chr);
	m_hCom = CreateFileA((chr),GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, NULL); 
	if (m_hCom == INVALID_HANDLE_VALUE) 
	{
		printf("CreateFile fail\n"); 
	} 
	else 
	{
		printf("CreateFile ok\n"); 
	}

	//2. ���ö�д������ 
	if(SetupComm(m_hCom,2048,2048)) 
	{
		printf("buffer ok\n"); 
	} 
	else 
	{
		printf("buffer fail\n"); 
	} 

	//3. ���ô��ڲ�����ʱ 
	//memset(&TimeOuts,0,sizeof(TimeOuts)); 
	TimeOuts.ReadIntervalTimeout         = 1000; 
	TimeOuts.ReadTotalTimeoutConstant    = 1000; 
	TimeOuts.ReadTotalTimeoutMultiplier = 1000; 
	TimeOuts.WriteTotalTimeoutConstant   = 1000; 
	TimeOuts.WriteTotalTimeoutMultiplier = 1000; 
	if(SetCommTimeouts(m_hCom,&TimeOuts)) 
	{
		printf("comm time_out ok!\n");
	}

	//4. ���ô��ڲ��� 
	if (GetCommState(m_hCom,&dcb)) 
	{
		printf("getcommstate ok\n"); 
	} 
	else 
	{
		printf("getcommstate fail\n"); 
	} 
	dcb.DCBlength = sizeof(dcb); 
	if (BuildCommDCB(_T("115200,n,8,1"),&dcb))//���ģãµ����ݴ����ʡ���żУ�����͡�����λ��ֹͣλ 
	{
		printf("buildcommdcb ok\n"); 
	} 
	else 
	{
		printf("buildcommdcb fail\n"); 
	}
	if(SetCommState(m_hCom,&dcb)) 
	{ 
		printf("setcommstate ok\n"); 
	} 
	else 
	{ 
		printf("setcommstate fail\n"); 
	}

	//5.��ʼ���첽�ṹ 
	ZeroMemory(&wrOverlapped,sizeof(wrOverlapped)); 
	wrOverlapped.Offset = 0; 
	wrOverlapped.OffsetHigh = 0;
	ZeroMemory(&wrOverlapped2,sizeof(wrOverlapped2)); 
	wrOverlapped2.Offset = 0; 
	wrOverlapped2.OffsetHigh = 0;

	if (wrOverlapped.hEvent != NULL) 
	{ 
		ResetEvent(wrOverlapped.hEvent); 
	} 
	if (wrOverlapped2.hEvent != NULL) 
	{ 
		ResetEvent(wrOverlapped2.hEvent); 
	} 
	wrOverlapped.hEvent = CreateEvent(NULL,TRUE,FALSE,NULL);//�����ֹ���λ�¼�
	wrOverlapped2.hEvent = CreateEvent(NULL,TRUE,FALSE,NULL);//�����ֹ���λ�¼�

	//����һ���߳�
	thread_data.vcc = wrOverlapped; ;
	thread_data.hcom = m_hCom;
	thread_data2.vcc = wrOverlapped2; 
	thread_data2.hcom = m_hCom;
	hThread1=CreateThread(NULL,0,RcvFromCom,(LPVOID)&thread_data,0,NULL); //��ע�⡻�����߳�
	hThread2=CreateThread(NULL,0,SendToCom,(LPVOID)&thread_data2,0,NULL); //��ע�⡻�����߳�

	LeaveCriticalSection(&com_cs);//�Ƴ��ٽ�� 
}

DWORD WINAPI RcvFromCom(LPVOID lParam) //�̵߳�ִ�к��� 
{

	Thread_Data* pThread_Data = (Thread_Data*)lParam; //���̺߳����������¶���Ľṹָ�� 
	HANDLE com= pThread_Data->hcom;       //ͨ���¶���Ľṹָ��ʹ��ԭ�ṹ�ĳ�Ա 
	OVERLAPPED wrOverlapped1 = pThread_Data->vcc;   //ͨ���¶���Ľṹָ��ʹ��ԭ�ṹ�ĳ�Ա 
	COMSTAT ComStat ;          //����һ������״̬�ṹ���󣬸��ṹ��10���������ھŸ����� 
	//DWORD cbInQue;          //��ʾ���뻺�����ж����ֽڵ����ݵ�� 
	DWORD word_count;          //���ʵ�ʶ����������ֽ������ 
	DWORD Event = 0; 
	DWORD CommEvtMask=0 ; 
	DWORD result; 
	DWORD error; 

	SetCommMask(com,EV_RXCHAR);//���ô��ڵȴ��¼�

	while(1) 
	{ 
		result = WaitCommEvent(com,&CommEvtMask,&wrOverlapped1);//�ȴ��¼����� 
		if(!result) 
		{ 
			switch(error = GetLastError()) 
			{ 
			case ERROR_IO_PENDING : 
				break; 
			case 87: 
				break; 
			default: 
				break; 
			} 
		} 
		else 
		{ 
			result = ClearCommError(com,&error,&ComStat); 
			if(ComStat.cbInQue == 0) 
			{ 
				continue; 
			} 
		}
		Event = WaitForSingleObject(wrOverlapped1.hEvent,INFINITE);//�첽����ȴ����� 
		EnterCriticalSection(&com_cs); 
		result = ClearCommError(com,&error,&ComStat); 
		if(ComStat.cbInQue == 0) 
		{ 
			continue; 
		}

		ReadFile(com,receive_data,ComStat.cbInQue,&word_count,&wrOverlapped1);//��ϵͳ���ջ������������յ����������� 
		if (ComStat.cbInQue>UART_LEN)
			continue;
		g_RcvBuf.writeInReceiveBuf((char*)receive_data, ComStat.cbInQue);
		LeaveCriticalSection(&com_cs);//�Ƴ��ٽ�� 
		//char2hex(receive_data, 15);
		if (5>8)
		{
			//send to com
			//g_SendMsgList.push_back(p);
		}
		for(int k=0;k<UART_LEN;k++) 
		{ 
			receive_data[k] = 0x00; 
		} 
	} 
	return 0; 
}

DWORD WINAPI SendToCom(LPVOID lParam) //�̵߳�ִ�к��� 
{

	Thread_Data* pThread_Data = (Thread_Data*)lParam; //���̺߳����������¶���Ľṹָ�� 
	HANDLE com= pThread_Data->hcom;       //ͨ���¶���Ľṹָ��ʹ��ԭ�ṹ�ĳ�Ա 
	OVERLAPPED wrOverlapped1 = pThread_Data->vcc;   //ͨ���¶���Ľṹָ��ʹ��ԭ�ṹ�ĳ�Ա 
	//COMSTAT ComStat ;          //����һ������״̬�ṹ���󣬸��ṹ��10���������ھŸ����� 
	//DWORD cbInQue;          //��ʾ���뻺�����ж����ֽڵ����ݵ�� 
	DWORD word_count=0;          //���ʵ�ʶ����������ֽ������ 
	DWORD Event = 0; 
	DWORD CommEvtMask=0 ; 

	while(1) 
	{ 	
		if (g_SendMsgList.size()==0)
		{
			continue;
		}
		UartData seData = g_SendMsgList.front();
		char pData[UART_LEN]= {0};
		int datalen=seData.len;
		memcpy(pData, seData.p, seData.len);
		g_SendMsgList.pop_front();

		EnterCriticalSection(&com_cs2); 
		//result = ClearCommError(com,&error,&ComStat); 
		/*if(ComStat.cbInQue == 0) 
		{ 
			continue; 
		}*/
		//int dddd=ComStat.cbInQue;
		WriteFile(com,pData,datalen,&word_count,&wrOverlapped1);
		//ReadFile(com,receive_data,ComStat.cbInQue,&word_count,&wrOverlapped1);//��ϵͳ���ջ������������յ����������� 
		LeaveCriticalSection(&com_cs2);//�Ƴ��ٽ�� 
		
	} 
	return 0; 
}

