#include "DSP2833x_Device.h"     // DSP2833x Headerfile Include File
#include "DSP2833x_Examples.h"   // DSP2833x Examples Include File

//�궨��
//#define DEBUG
#define	CPU_CLK	150e6
#define	AD7606_BASIC	(*((volatile  Uint16 *)0x200000))
#define AD_BUSY			GpioDataRegs.GPBDAT.bit.GPIO60
#define AD_CONVST		GpioDataRegs.GPBSET.bit.GPIO61=1
#define CLEAR_ADCON		GpioDataRegs.GPBCLEAR.bit.GPIO61=1
#define AD_RST			GpioDataRegs.GPBSET.bit.GPIO59=1
#define CLR_ADRST		GpioDataRegs.GPBCLEAR.bit.GPIO59=1
#define	LDAC_LOW		GpioDataRegs.GPBCLEAR.bit.GPIO51=1
#define	LDAC_HIGH		GpioDataRegs.GPBSET.bit.GPIO51=1
#define	LED1			GpioDataRegs.GPADAT.bit.GPIO0
#define	LED2			GpioDataRegs.GPADAT.bit.GPIO1
#define	LED3			GpioDataRegs.GPADAT.bit.GPIO2
#define	LED4			GpioDataRegs.GPADAT.bit.GPIO3
#define	PRESAMP			GpioDataRegs.GPADAT.bit.GPIO4
#define FLOSAMP			GpioDataRegs.GPADAT.bit.GPIO6
#define CMD_ZERO		(int32)0x1745D1
#define CMD_SCALE		(float32)(0xFFFFFF/11)*10
#define INTE_SEP		(int32)381300	//���ַ������
#define PREUMAX			(int32)0xFFFFFF	//ѹ��������ֵ����ֹ���ֱ���
#define PREUMIN			0				//ѹ�������Сֵ����ֹ���ֱ���

#define	BUILD_UINT32(loByte, miByte, hiByte)	\
	((Uint32)(((Uint32)(loByte) & 0x000000FF) + (((Uint32)(miByte) & 0x000000FF) << 8) +(((Uint32)(hiByte) & 0x000000FF) << 16)))
#define BUILD_UINT16(loByte, hiByte) \
          ((uint16)(((loByte) & 0x00FF) + (((hiByte) & 0x00FF) << 8)))
#define HI_UINT32(a) (((a) >> 16) & 0xFF)
#define MI_UINT32(a) (((a) >> 8) & 0xFF)
#define LO_UINT32(a) ((a) & 0xFF)

// Prototype statements for functions found within this file.
void INTConfig(void);
void InitXintf(void);
void SendData(unsigned char *buf);
void SendCmd(unsigned char *buf);
void CfgScia(void);
void CfgScib(void);
void CfgScic(void);
void CfgCap(void);
void CfgTimer0(void);
void CfgTimer1(void);
void CfgTimer2(void);
void CfgLed(void);
void InitParameter(void);
void ADInit(void);
void spi_xmit(Uint16 a);	//DA Ouptput Function, you can directly use it without adding delay
void spi_fifo_init(void);
void spi_init(void);
unsigned char XorCheckSum(unsigned char * pBuf, char len);

interrupt void sciaTxFifoIsr(void);
interrupt void sciaRxFifoIsr(void);
interrupt void scicTxFifoIsr(void);
interrupt void scibTxFifoIsr(void);
interrupt void scibRxFifoIsr(void);
interrupt void ISRCap1(void);
interrupt void ISRCap2(void);
interrupt void ISRTimer0(void);
interrupt void ISRTimer1(void);
interrupt void ISRTimer2(void);

// Global counts used in this example
unsigned char sdataA[8];    // Send data for SCI-A
unsigned char rdataA[8];    // Received data for SCI-A
unsigned char sdataC[12];		//Send data for SCI-C
char tst[]="abcdefghijkl";
char cmdupdate=0;	//0Ϊδ���£�1Ϊ�Ѹ���
char SWITCH_FLAG;
int32 pre_cmd=0;	//����ѹ��
int32 flow_cmd=0;	//��������
unsigned char pre_out[3]={0};	//ѹ�����
unsigned char flow_out[3]={0};	//�������
unsigned char CMD[12];	//���������
Uint32 T1=0,T2=0,T3=0,T4=0;
int32 pre_sensor=0, flow_sensor=0;
unsigned char pre_cmd_data[8], pre_act_data[8];
unsigned char flow_cmd_data[8], flow_act_data[8];
//PID����
//int32 err_pre[3];
int32 err_flow[3];
float32 kp_pre, kd_pre, ki_pre;
float32 kp_flow, kd_flow, ki_flow;
int32 pre_cal[2], flow_cal[2];
int32 pre_32_out, flow_32_out;
int32 kd_tmp_pre, kd_tmp_flow;
//AD\DA
Uint16 ad[4];
int	gStatus;
unsigned char precmd_change[2];//���ԣ����ڱ����λ����ť�ĸı�ֵ


struct	pid{
	int32	SetCmd;//�����趨ֵ
	//int32	OutCmd;//ʵ�����ֵ
	int32	Sensor;//����������
	int32 	err;	//����ƫ��ֵ
	int32 	err_next;//������һ��ƫ��ֵ
	int32 	err_last;//�����������ƫ��ֵ
	int32	integral;//���λ������ò���
	float32 Kp,Ki,Kd;//������������֡�΢��ϵ��
}PrePID, FlowPID;

//float32 tmp;

void main(void)
{
// Step 1. Initialize System Control:
// PLL, WatchDog, enable Peripheral Clocks
// This example function is found in the DSP2833x_SysCtrl.c file.
   InitSysCtrl();
   InitXintf(); //ADоƬ��ȡ
   InitXintf16Gpio();

// Step 2. Initalize GPIO:
// This example function is found in the DSP2833x_Gpio.c file and
// illustrates how to set the GPIO to it's default state.
   // InitGpio(); Skipped for this example

   InitSciGpio();
   InitSpiaGpio(); //OLED��
// Step 3. Clear all interrupts and initialize PIE vector table:
// Disable CPU interrupts
   DINT;

// Initialize PIE control registers to their default state.
// The default state is all PIE interrupts disabled and flags
// are cleared.
// This function is found in the DSP2833x_PieCtrl.c file.
   InitPieCtrl();

// Disable CPU interrupts and clear all CPU interrupt flags:
   IER = 0x0000;
   IFR = 0x0000;

// Initialize the PIE vector table with pointers to the shell Interrupt
// Service Routines (ISR).
// This will populate the entire table, even if the interrupt
// is not used in this example.  This is useful for debug purposes.
// The shell ISR routines are found in DSP2833x_DefaultIsr.c.
// This function is found in DSP2833x_PieVect.c.
   InitPieVectTable();

// Step 4. Initialize all the Device Peripherals:
// This function is found in DSP2833x_InitPeripherals.c
// InitPeripherals(); // Not required for this example

   EALLOW;	// This is needed to write to EALLOW protected registers
   PieVectTable.SCIRXINTA = &sciaRxFifoIsr;
   PieVectTable.SCITXINTA = &sciaTxFifoIsr;
   PieVectTable.SCITXINTC = &scicTxFifoIsr;
   PieVectTable.SCIRXINTB = &scibRxFifoIsr;
   PieVectTable.SCITXINTB = &scibTxFifoIsr;
   PieVectTable.ECAP1_INT = &ISRCap1;
   PieVectTable.ECAP2_INT = &ISRCap2;
   PieVectTable.TINT0 = &ISRTimer0;
   PieVectTable.XINT13 = &ISRTimer1;
   PieVectTable.TINT2 = &ISRTimer2;
   EDIS;   // This is needed to disable write to EALLOW protected registers

   InitParameter();//��ʼ������
   InitCpuTimers();//��ʼ����ʱ��
   InitSci();  // Initalize SCI
   ADInit();	//Initalize AD7606
   InitCapl();
   spi_init();
   spi_fifo_init();

   INTConfig();

   AD_RST;
   DELAY_US(100000);
   CLR_ADRST;

   ConfigCpuTimer(&CpuTimer0, 150, 7500); //AD ��DA��
   ConfigCpuTimer(&CpuTimer1, 150, 5000);
   ConfigCpuTimer(&CpuTimer2, 150, 50000);
   StartCpuTimer2();

   StopCpuTimer0();
   StopCpuTimer1();
   //StopCpuTimer2();

#ifdef DEBUG
   PRESAMP=1;
   DELAY_US(10);
   FLOSAMP=1;
   DELAY_US(10);
   LED1=1;
   DELAY_US(10);
   LED2=1;
   DELAY_US(10);
   LED3=1;
   DELAY_US(10);
   LED4=1;
   DELAY_US(10);
#endif

   while(1)
   {
	   if(gStatus==100)
	   {
		   StopCpuTimer0();
		   SWITCH_FLAG=0;
		   gStatus=0;
	   }
#ifdef DEBUG
	   if(rdataA[0]==0x5A)
	   {
		   //CfgScic();
		   SendData(rdataA);
	   }
	   else
	   {
		   //INTConfig();
		   SendCmd(tst);
	   }
	   //SciaRegs.SCIFFTX.bit.TXFFINT=1;
	   //SendData(tst);
	   DELAY_US(1000000);
#endif
   }

}

//������ʼ��
void InitParameter(void)
{
	char i;
	//����������������ʵ���������
	SWITCH_FLAG=0;
	gStatus=0;
	PrePID.Kp = 0.2;
	PrePID.Ki = 0.04;
	PrePID.Kd = 0.2;
	PrePID.SetCmd = 0;
	PrePID.Sensor = 0;
	PrePID.err = 0;
	PrePID.err_last = 0;
	PrePID.err_next = 0;
	PrePID.integral = 0;
	//kp_pre = 0.2;//PID����
	//ki_pre = 0.04;
	//kd_pre = 0.2;
	pre_cal[0]=0;
	pre_cal[1]=0;
	FlowPID.Kp = 0.2;
	FlowPID.Ki = 0.04;
	FlowPID.Kd = 0.2;
	FlowPID.SetCmd = 0;
	FlowPID.Sensor = 0;
	FlowPID.err = 0;
	FlowPID.err_last = 0;
	FlowPID.err_next = 0;
	FlowPID.integral = 0;
	//kp_flow = 0.2;//PID����
	//ki_flow = 0.04;
	//kd_flow = 0.2;
	flow_cal[0]=0;
	flow_cal[1]=0;
	for(i=0;i<8;i++)
		rdataA[i]=0;
	for(i=0;i<8;i++)
		sdataA[i]=0;
	for(i=0;i<12;i++)
		sdataC[i]=0;
	for(i=0;i<8;i++)
		pre_cmd_data[i]=0;
	for(i=0;i<8;i++)
		pre_act_data[i]=0;
	for(i=0;i<8;i++)
		flow_cmd_data[i]=0;
	for(i=0;i<8;i++)
		flow_act_data[i]=0;
	for(i=0;i<3;i++)
		err_flow[i]=0;
	for(i=0;i<12;i++)
		CMD[i]=0;
	for(i=0;i<3;i++)
		pre_out[i]=0;
	for(i=0;i<3;i++)
		flow_out[i]=0;
	pre_32_out=0;
	flow_32_out=0;
	//ָ���ʼ��
	CMD[0]=0xAA;
	CMD[1]=0xBB;
	CMD[2]=9;
	pre_cmd_data[0]=0x5A;//��������ָ��
	pre_cmd_data[1]=0xB2;
	pre_cmd_data[2]=0xAA;
	pre_cmd_data[7]=0x23;
	pre_act_data[0]=0x5A; //����ʵ������
	pre_act_data[1]=0xB2;
	pre_act_data[2]=0x55;
	pre_act_data[7]=0x23;
	flow_cmd_data[0]=0x5A;
	flow_cmd_data[1]=0xB4;
	flow_cmd_data[2]=0xAA;
	flow_cmd_data[7]=0x23;
	flow_act_data[0]=0x5A;
	flow_act_data[1]=0xB4;
	flow_act_data[2]=0x55;
	flow_act_data[7]=0x23;
}

void ADInit(void)
{
	EALLOW;
	GpioCtrlRegs.GPBMUX2.bit.GPIO51 = 0; //DA��LDAC����
	GpioCtrlRegs.GPBDIR.bit.GPIO51 = 1;
	GpioCtrlRegs.GPBMUX2.bit.GPIO59=0;
	GpioCtrlRegs.GPBDIR.bit.GPIO59=1;
	GpioCtrlRegs.GPBMUX2.bit.GPIO61 = 0;  // XWE0
	GpioCtrlRegs.GPBDIR.bit.GPIO61=1;
	GpioCtrlRegs.GPBMUX2.bit.GPIO60=0; //ADBUSY
	GpioCtrlRegs.GPBDIR.bit.GPIO60=0;//INPUT
	GpioCtrlRegs.GPBQSEL2.bit.GPIO60= 0;
	EDIS;
}

/*********************************************************************************************************
** ��������: spi_fifo_init
** ��������: SPI��ʼ������
** �䡡��: ��
** �䡡�� : ��
** ȫ�ֱ���:
** ����ģ��:
** ״     ̬: ���������
** ������: �˿���
** �ա���: 20151108
**-------------------------------------------------------------------------------------------------------
** �޸���:
** �ա���:
**-------------------------------------------------------------------------------------------------------
********************************************************************************************************/
void spi_init(void)
{
	SpiaRegs.SPICCR.all =0x004F;	             // Reset on, falling edge, 16-bit char bits
	SpiaRegs.SPICTL.all =0x0006;    		     // Enable master mode, normal phase,
                                                 // enable talk, and SPI int disabled.
	SpiaRegs.SPIBRR =0x007F;					//ȷ��SPICLK
    SpiaRegs.SPICCR.all =0x00CF;		         // Relinquish SPI from Reset
    SpiaRegs.SPIPRI.bit.FREE = 1;                // Set so breakpoints don't disturb xmission
}

/*********************************************************************************************************
** ��������: spi_xmit
** ��������: SPI����
** �䡡��: ��
** �䡡�� : ��
** ȫ�ֱ���:
** ����ģ��:
** ״     ̬: ���������
** ������: �˿���
** �ա���: 20151108
**-------------------------------------------------------------------------------------------------------
** �޸���:
** �ա���:
**-------------------------------------------------------------------------------------------------------
********************************************************************************************************/
void spi_xmit(Uint16 a)
{
    SpiaRegs.SPITXBUF=a;
    DELAY_US(70);
    LDAC_LOW;
    DELAY_US(10);
    LDAC_HIGH;
}

/*********************************************************************************************************
** ��������: spi_fifo_init
** ��������: SPI��FIFO��ʼ������
** �䡡��: ��
** �䡡�� : ��
** ȫ�ֱ���:
** ����ģ��:
** ״     ̬: ���������
** ������: �˿���
** �ա���: 20151108
**-------------------------------------------------------------------------------------------------------
** �޸���:
** �ա���:
**-------------------------------------------------------------------------------------------------------
********************************************************************************************************/
void spi_fifo_init(void)
{
// Initialize SPI FIFO registers
    SpiaRegs.SPIFFTX.all=0xE040;
    SpiaRegs.SPIFFRX.all=0x204f;
    SpiaRegs.SPIFFCT.all=0x0;
}


//����ָ�����
/*********************************************************************************************************
** ��������: sciaTxFifoIsr
** ��������: SCIA�����жϴ�����
** �䡡��: ��
** �䡡�� : ��
** ȫ�ֱ���:
** ����ģ��:
** ״     ̬: һЩ������Ҫ�ڵ�����ȷ��
** ������: �˿���
** �ա���: 20151108
**-------------------------------------------------------------------------------------------------------
** �޸���:
** �ա���:
**-------------------------------------------------------------------------------------------------------
********************************************************************************************************/
interrupt void sciaTxFifoIsr(void)
{
	unsigned char fifocnt;
	for(fifocnt=0; fifocnt< 8; fifocnt++)
	{
		SciaRegs.SCITXBUF=sdataA[fifocnt];     // Send data
	}
	//SciaRegs.SCIFFTX.bit.TXFFINTCLR=1;  // ��˱�־λ��ʾ�жϿ��Բ���
	PieCtrlRegs.PIEACK.bit.ACK9=1;      // Issue PIE ACK
}

/*********************************************************************************************************
** ��������: sciaRxFifoIsr
** ��������: SCIB�����жϴ�����������ָ����������÷�ʽ��
** �䡡��: ��
** �䡡�� : ��
** ȫ�ֱ���:
** ����ģ��:
** ״     ̬: Ϊ��SCIBͬ��
** ������: �˿���
** �ա���: 20151108
**-------------------------------------------------------------------------------------------------------
** �޸���:
** �ա���:
**-------------------------------------------------------------------------------------------------------
********************************************************************************************************/
interrupt void sciaRxFifoIsr(void)
{
	unsigned char fifocnt,checksum,len;
    DINT;//�����ж�

	for(fifocnt=0;fifocnt<8;fifocnt++)
	{
	   rdataA[fifocnt]=SciaRegs.SCIRXBUF.all;	 // Read data
	}

	if(rdataA[0]==0x5A)
	{
		len=6;
		checksum=XorCheckSum(rdataA, len);
		if(checksum == rdataA[len] && rdataA[len+1] == 0x23)
		{
			switch(rdataA[1])
			{
				//����ֹͣѹ������
				case 0xB1:
					SWITCH_FLAG=0;//PID����Ƴ�����л�����
					if(rdataA[2]==0xAA)
					{
						PrePID.SetCmd = BUILD_UINT32(rdataA[5], rdataA[4], rdataA[3]);
						StopCpuTimer1();
						StartCpuTimer0();	//������ʱ������ʼ����PID����
					}
					else if(rdataA[2]==0x55)
					{
						StopCpuTimer0();
						StopCpuTimer1();
						//SendCmd(tst);
					}
					break;
				//����ֹͣ��������
				case 0xB3:
					SWITCH_FLAG=0;
					if(rdataA[2]==0xAA)//����
					{
						flow_cmd = BUILD_UINT32(rdataA[5], rdataA[4], rdataA[3]);
						StopCpuTimer0();
						StartCpuTimer1();	//������ʱ������ʼ����PID����
					}
					else if(rdataA[2]==0x55)//ֹͣ
					{
						StopCpuTimer0();
						StopCpuTimer1();
						//SendCmd(tst);
					}
					break;
				//ѹ��PID�����޸�
				case 0xB5:
					kp_pre=rdataA[3];
					ki_pre=(float32)(rdataA[4]/1000);
					kd_pre=rdataA[5];
					SWITCH_FLAG=1;
					PrePID.SetCmd=0xFFFFFF;
					StartCpuTimer0();
					break;
				//����PID�����޸�
				case 0xB6:
					kp_flow=rdataA[3];
					ki_flow=(float32)(rdataA[4]/1000);
					kd_flow=rdataA[5];
					SWITCH_FLAG=1;
					flow_cmd=0xFFFFFF;
					StartCpuTimer1();
					break;
				//ѹ��΢��
				case 0xB9:
					PrePID.SetCmd+=0x2F8;//������ȷ��
					break;
				//����΢��
				case 0xBA:
					flow_cmd+=0x2FB;
					break;
				//��ͣ
				case 0xBB:
					StopCpuTimer0();
					StopCpuTimer1();
					CMD[3]=0;//Close Channel 1
					CMD[4]=0;
					CMD[5]=0;
					CMD[6]=0;//Close Channel 2
					CMD[7]=0;
					CMD[8]=0;
					CMD[9]=0;//Close Channel 3
					CMD[10]=0;
					CMD[11]=0;
					SendCmd(CMD);
					break;
				//����PID���ڽ���
				case 0xC1:
					StopCpuTimer0();
					StopCpuTimer1();
					break;
				default:
					break;
				}
			}
	}
	//SendData(rdataA);
	//SciaRegs.SCIFFTX.bit.TXFFINTCLR=1;  // Clear Interrupt flag
	SciaRegs.SCIFFRX.bit.RXFFOVRCLR=1;  // Clear Overflow flag
	SciaRegs.SCIFFRX.bit.RXFFINTCLR=1; 	// Clear Interrupt flag
	PieCtrlRegs.PIEACK.bit.ACK9=1;  	// Issue PIE ack
	EINT;//�����ж�
}

/*********************************************************************************************************
** ��������: scibTxFifoIsr
** ��������: SCIB�����жϴ�����
** �䡡��: ��
** �䡡�� : ��
** ȫ�ֱ���:
** ����ģ��:
** ״     ̬: ���������
** ������: �˿���
** �ա���: 20151108
**-------------------------------------------------------------------------------------------------------
** �޸���:
** �ա���:
**-------------------------------------------------------------------------------------------------------
********************************************************************************************************/
interrupt void scibTxFifoIsr(void)
{
	unsigned char fifocnt;
	for(fifocnt=0; fifocnt< 8; fifocnt++)
	{
		ScibRegs.SCITXBUF=sdataA[fifocnt];     // Send data
	}
	//SciaRegs.SCIFFTX.bit.TXFFINTCLR=1;  // ��˱�־λ��ʾ�жϿ��Բ���
	PieCtrlRegs.PIEACK.bit.ACK9=1;      // Issue PIE ACK
}

//����ָ�����
/*********************************************************************************************************
** ��������: scibRxFifoIsr
** ��������: SCIB�����жϴ�����������ָ�����
** �䡡��: ��
** �䡡�� : ��
** ȫ�ֱ���:
** ����ģ��:
** ״     ̬: һЩ������Ҫ�ڵ�����ȷ��
** ������: �˿���
** �ա���: 20151108
**-------------------------------------------------------------------------------------------------------
** �޸���:
** �ա���:
**-------------------------------------------------------------------------------------------------------
********************************************************************************************************/
interrupt void scibRxFifoIsr(void)
{
	unsigned char fifocnt,checksum,len;
    DINT;//�����ж�

	for(fifocnt=0;fifocnt<8;fifocnt++)
	{
	   rdataA[fifocnt]=ScibRegs.SCIRXBUF.all;	 // Read data
	}

	if(rdataA[0]==0x5A)
	{
		len=6;
		checksum=XorCheckSum(rdataA, len);
		if(checksum == rdataA[len] && rdataA[len+1] == 0x23)
		{
			switch(rdataA[1])
			{
				//����ֹͣѹ������
				case 0xB1:
					SWITCH_FLAG=0;//PID����Ƴ�����л�����
					if(rdataA[2]==0xAA)
					{
						PrePID.SetCmd = BUILD_UINT32(rdataA[5], rdataA[4], rdataA[3]);
						StopCpuTimer1();
						StartCpuTimer0();	//������ʱ������ʼ����PID����
					}
					else if(rdataA[2]==0x55)
					{
						StopCpuTimer0();
						StopCpuTimer1();
						//SendCmd(tst);
						//Ҫ��Ҫ���Ϸ���0����Դ��ָ��
					}
					break;
				//����ֹͣ��������
				case 0xB3:
					SWITCH_FLAG=0;
					if(rdataA[2]==0xAA)//����
					{
						flow_cmd = BUILD_UINT32(rdataA[5], rdataA[4], rdataA[3]);
						StopCpuTimer0();
						StartCpuTimer1();	//������ʱ������ʼ����PID����
					}
					else if(rdataA[2]==0x55)//ֹͣ
					{
						StopCpuTimer0();
						StopCpuTimer1();
						//SendCmd(tst);
					}
					break;
				//ѹ��PID�����޸�
				case 0xB5://���ݸ�ʽ����趨,NEEDs to DEFINE
					PrePID.Kp=(float32)(rdataA[3]/10);
					PrePID.Ki=(float32)(rdataA[4]/100);
					PrePID.Kd=(float32)(rdataA[5]/10);
					SWITCH_FLAG=1;
					PrePID.SetCmd=0xFFFFFF;
					if(rdataA[2]==0xAA)//Ӧ�ò��ҽ���PID����
						StartCpuTimer0();
					//���������Ӧ�ã������в���

					break;
				//����PID�����޸ģ���ȷ��
				case 0xB6:
					kp_flow=rdataA[3];
					ki_flow=(float32)(rdataA[4]/1000);
					kd_flow=rdataA[5];
					SWITCH_FLAG=1;
					flow_cmd=0xFFFFFF;
					StartCpuTimer1();
					break;
				//ѹ��΢��
				case 0xB9:
					if(rdataA[2]==0xAA)//Micro regulate Plus
						PrePID.SetCmd+=0x129E4;//This parameter is determined by the precision of the pressure adjustment.
					else if(rdataA[2]==0x55)//Micro regulate Minus
						PrePID.SetCmd-=0x129E4;
					break;
				//����΢��
				case 0xBA://�˴�������ȷ��
					if(rdataA[2]==0xAA)//Micro regulate Plus
						FlowPID.SetCmd+=0x129E4;//This parameter is determined by the precision of the Flow adjustment.
					else if(rdataA[2]==0x55)//Micro regulate Minus
						FlowPID.SetCmd-=0x129E4;
					flow_cmd+=0x2FB;
					break;
				//��ͣ,��ͣ��ʽҲ��Ҫ����ʵ�����ȷ��
				case 0xBB:
					StopCpuTimer0();
					StopCpuTimer1();
					CMD[3]=0;//Close Channel 1
					CMD[4]=0;
					CMD[5]=0;
					CMD[6]=0;//Close Channel 2
					CMD[7]=0;
					CMD[8]=0;
					CMD[9]=0;//Close Channel 3
					CMD[10]=0;
					CMD[11]=0;
					SendCmd(CMD);
					break;
				default:
					break;
				}
			}
	}
	//SendData(rdataA);
	//ScibRegs.SCIFFTX.bit.TXFFINTCLR=1;  // Clear Interrupt flag
	ScibRegs.SCIFFRX.bit.RXFFOVRCLR=1;  // Clear Overflow flag
	ScibRegs.SCIFFRX.bit.RXFFINTCLR=1; 	// Clear Interrupt flag
	PieCtrlRegs.PIEACK.bit.ACK9=1;  	// Issue PIE ack
	EINT;//�����ж�
}

/*********************************************************************************************************
** ��������: scicTxFifoIsr
** ��������: SCIC�����жϴ�����
** �䡡��: ��
** �䡡�� : ��
** ȫ�ֱ���:
** ����ģ��:
** ״     ̬: ���������
** ������: �˿���
** �ա���: 20151108
**-------------------------------------------------------------------------------------------------------
** �޸���:
** �ա���:
**-------------------------------------------------------------------------------------------------------
********************************************************************************************************/
interrupt void scicTxFifoIsr(void)
{
	unsigned char fifocnt;
	for(fifocnt=0; fifocnt< 12; fifocnt++)
	{
		ScicRegs.SCITXBUF=sdataC[fifocnt];     // Send data
	}
		//SciaRegs.SCIFFTX.bit.TXFFINTCLR=1;  // ��˱�־λ��ʾ�жϿ��Բ���
	PieCtrlRegs.PIEACK.bit.ACK8=1;      // Issue PIE ACK
}

/*********************************************************************************************************
** ��������: ISRCap1
** ��������: eCap�жϴ�����������ѹ�����������ݲɼ�
** �䡡��: ��
** �䡡�� : ��
** ȫ�ֱ���:pre_sensor
** ����ģ��:
** ״     ̬: ��������ɣ�������
** ������: �˿���
** �ա���: 20151108
**-------------------------------------------------------------------------------------------------------
** �޸���:
** �ա���:
**-------------------------------------------------------------------------------------------------------
********************************************************************************************************/
interrupt void ISRCap1(void)
{
	Uint32 t1=0,t2=0,t3=0,t4=0;
	int32	pre_freq;
	//float32 tmp;
   // Acknowledge this interrupt to receive more interrupts from group 1
    PieCtrlRegs.PIEACK.all = PIEACK_GROUP4;
    ECap1Regs.ECCLR.all=0xFFFF;//clare all flag
	t1= ECap1Regs.CAP1;
	t2= ECap1Regs.CAP2;
	t3= ECap1Regs.CAP3;
  	t4= ECap1Regs.CAP4;
    T1=t2-t1;T2=t4-t3;
    pre_freq = (int32)((float32)(CPU_CLK/T1)*10);//��ȷ��0.1Hz
    //tmp = ((float32)(pre_sensor-10000)/40000);
    //pre_sensor = (int32)(((float32)(pre_freq-10000)/40000)*(float32)CMD_SCALE) + CMD_ZERO;
    PrePID.Sensor = (int32)(((float32)(pre_freq-10000)/40000)*CMD_SCALE) + CMD_ZERO;
}

/*********************************************************************************************************
** ��������: ISRCap2
** ��������: eCap�жϴ������������������������ݲɼ�
** �䡡��: ��
** �䡡�� : ��
** ȫ�ֱ���:flow_sensor
** ����ģ��:
** ״     ̬: ��������ɣ�������
** ������: �˿���
** �ա���: 20151108
**-------------------------------------------------------------------------------------------------------
** �޸���:
** �ա���:
**-------------------------------------------------------------------------------------------------------
********************************************************************************************************/
interrupt void ISRCap2(void)
{
	Uint32 t5,t6,t7,t8;
	int32 flow_freq;
   // Acknowledge this interrupt to receive more interrupts from group 1
    PieCtrlRegs.PIEACK.all = PIEACK_GROUP4;
    ECap2Regs.ECCLR.all=0xFFFF;//clear all flag
	t5= ECap2Regs.CAP1;
	t6= ECap2Regs.CAP2;
	t7= ECap2Regs.CAP3;
  	t8= ECap2Regs.CAP4;
    T3=t6-t5;T4=t8-t7;
    flow_freq = (int32)((float32)(CPU_CLK/T3)*10);//��ȷ��0.1Hz,�����ȷ��0.1Hz����1Hz��Ҫ���Ǳ�ȷ��
    flow_sensor = (int32)(((float32)(flow_freq-150000)/350000)*CMD_SCALE) + CMD_ZERO;//�������������Ƶ�ʷ�Χ15~50KHz
}

/*********************************************************************************************************
** ��������: ISRTimer0
** ��������: ��ʱ��0�жϴ�����������ѹ��PID����
** �䡡��: ��
** �䡡�� : ��
** ȫ�ֱ���: PrePID��kd_tmp_pre��pre_cal��pre_32_out��CMD��pre_act_data
** ����ģ��: SendCmd��SendData
** ״     ̬: ��������ɣ�������
** ������: �˿���
** �ա���: 20151108
**-------------------------------------------------------------------------------------------------------
** �޸���:
** �ա���:
**-------------------------------------------------------------------------------------------------------
********************************************************************************************************/
interrupt void ISRTimer0(void)
{
	char index;
	unsigned char pre_change[8];
	DINT;
	PieCtrlRegs.PIEACK.all = PIEACK_GROUP1;
	CpuTimer0Regs.TCR.bit.TIF=1;	//��ʱ����ָ��ʱ�䣬�ñ�־λ
	CpuTimer0Regs.TCR.bit.TRB=1;	//����Timer0�Ķ�ʱ����

	if(PRESAMP==0)//ִ��ѹ��PID����
	{
		PrePID.err = PrePID.SetCmd - PrePID.Sensor;//��ʱ��pre_sensor�Ѿ�ת���ɺ�0xFFFFFFһ���ĵ�λ
		//���ַ�����㷨�������PID�ڻ��ַ����㷨��ʵ��
		if(abs(PrePID.err)>INTE_SEP)//INTE_SEP����ʵ���������
			index = 0;
		else
		{
			index = 1;
			PrePID.integral += PrePID.err;//���λ��������̬����
		}
		kd_tmp_pre = (PrePID.err-2*PrePID.err_next+PrePID.err_last);
		pre_cal[1] = PrePID.Kp*(PrePID.err-PrePID.err_next)+index*PrePID.Ki*PrePID.integral/2+PrePID.Kd*kd_tmp_pre+pre_cal[0];
		pre_cal[0]=pre_cal[1];
		//pre_32_out = pre_cmd+pre_cal[1];
		pre_32_out = pre_32_out+pre_cal[1];//�����pre_cal[1]��Ӧ���Ǳ���ִ�еĿ�����������������Ҫ�����ϴε����ֵ��
		//�˴����жϣ����п����ֱ��͹���
		if(pre_32_out>PREUMAX)
		{
			pre_32_out = 0xFFFFFF;
		}
		else if(pre_32_out<PREUMIN)
		{
			pre_32_out = 0;
		}
		//��һ��ʵ�ֱ����PID
		PrePID.err_last = PrePID.err_next;
		PrePID.err_next = PrePID.err;

		pre_out[0] = HI_UINT32(pre_32_out);
		pre_out[1] = MI_UINT32(pre_32_out);
		pre_out[2] = LO_UINT32(pre_32_out);
		//���CMD��ֵ����
		CMD[3]=pre_out[0];//���CMD��ֵ����
		CMD[4]=pre_out[1];
		CMD[5]=pre_out[2];
		CMD[6]=0xFF;//Open Channel 2 MAX
		CMD[7]=0xFF;
		CMD[8]=0xFF;
		CMD[9]=0;//Close Channel 3
		CMD[10]=0;
		CMD[11]=0;
		SendCmd(CMD);
		//����Ĵ���Ŀ���Ǹ�����λ������������ֵ
		pre_change[0]=0x5A;
		pre_change[1]=0xC2;
		pre_change[2]=0xAA;
		pre_change[3]=0;
		pre_change[4]=precmd_change[0];
		pre_change[5]=precmd_change[1];
		pre_change[6]=XorCheckSum(pre_change, 6);
		pre_change[7]=0x23;
		SendData(pre_change);

		if(SWITCH_FLAG==1)
		{
			gStatus++;
			if(gStatus==101)
				gStatus=0;
		}
	}
	else//��ȡѹ����������ֵ
	{
#ifdef DEBUG
		pre_cmd_data[3] = HI_UINT32(pre_cal[1]);
		pre_cmd_data[4] = MI_UINT32(pre_cal[1]);
		pre_cmd_data[5] = LO_UINT32(pre_cal[1]);
		pre_cmd_data[6] = XorCheckSum(pre_cmd_data,6);
		SendData(pre_cmd_data);//�����������
#endif
		if(SWITCH_FLAG==0)
			pre_act_data[1]=0xB2;
		else
			pre_act_data[1]=0xB7;
		pre_act_data[3] = HI_UINT32(PrePID.Sensor);
		pre_act_data[4] = MI_UINT32(PrePID.Sensor);
		pre_act_data[5] = LO_UINT32(PrePID.Sensor);
		pre_act_data[6] = XorCheckSum(pre_act_data,6);
		SendData(pre_act_data);
		//���ʹ���������
	}
#ifdef DEBUG
	LED1=~LED1;
	//LED2=~LED2;
	//LED3=~LED3;
	//LED4=~LED4;
#endif
	PRESAMP=~PRESAMP;
	EINT;
}

/*********************************************************************************************************
** ��������: ISRTimer1
** ��������: ��ʱ��1�жϴ���������������PID����
** �䡡��: ��
** �䡡�� : ��
** ȫ�ֱ���: flow_out��err_flow��CMD��flow_act_data
** ����ģ��: SendCmd��SendData
** ״     ̬: PID���ڹ��̴���δ�����޸ģ�	�˴������������ֵ���´���û�м���
** ������: �˿���
** �ա���: 20151108
**-------------------------------------------------------------------------------------------------------
** �޸���:
** �ա���:
**-------------------------------------------------------------------------------------------------------
********************************************************************************************************/
interrupt void ISRTimer1(void)
{
	DINT;
	PieCtrlRegs.PIEACK.all = PIEACK_GROUP1;
	CpuTimer1Regs.TCR.bit.TIF=1;	//��ʱ����ָ��ʱ�䣬�ñ�־λ
	CpuTimer1Regs.TCR.bit.TRB=1;	//����Timer1�Ķ�ʱ����
	//FLOSAMPΪIO���ţ�������ÿ��һ�ζ�ʱ���жϣ�״̬�ı䣬�Ӷ���������źţ�����Ϊ�ߵ�ƽ�ɼ�����������
	if(FLOSAMP==0)//ִ������PID���ڹ���
	{
		//�˴�PID���ڹ��̴���δ�����޸�20151108
		//�˴������������ֵ���´���û�м���20151108
		err_flow[0] = err_flow[1];
		err_flow[1] = err_flow[2];
		err_flow[2] = flow_cmd - flow_sensor;
		flow_cal[0]=flow_cal[1];
		kd_tmp_flow = (err_flow[2]-2*err_flow[1]+err_flow[0]);
		flow_cal[1] = kp_flow*(err_flow[2]-err_flow[1])+ki_flow*err_flow[2]+kd_flow*kd_tmp_flow+flow_cal[0];
		flow_32_out = flow_cmd + flow_cal[1];
		flow_out[0] = HI_UINT32(flow_32_out);
		flow_out[1] = MI_UINT32(flow_32_out);
		flow_out[2] = LO_UINT32(flow_32_out);
		CMD[3]=0xFF;//���CMD��ֵ����
		CMD[4]=0xFF;//This number needs to be confirmed
		CMD[5]=0xFF;
		CMD[6]=flow_out[0];//Open Channel 2 MAX
		CMD[7]=flow_out[1];
		CMD[8]=flow_out[2];
		CMD[9]=0;//Close Channel 3
		CMD[10]=0;
		CMD[11]=0;
		SendCmd(CMD);//����������
	}
	else//��ȡ������������ֵ
	{
		if(SWITCH_FLAG==0)
			flow_act_data[1]=0xB4;
		else
			flow_act_data[1]=0xB8;
		//���ʹ���������
		flow_act_data[3] = HI_UINT32(flow_sensor);
		flow_act_data[4] = MI_UINT32(flow_sensor);
		flow_act_data[5] = LO_UINT32(flow_sensor);
		flow_act_data[6] = XorCheckSum(flow_act_data,6);
		SendData(flow_act_data);
		//���ʹ���������
	}
#ifdef DEBUG
	//LED1=~LED1;
	//LED2=~LED2;
	LED3=~LED3;
	LED4=~LED4;
#endif
	FLOSAMP=~FLOSAMP;
	EINT;
}

/*********************************************************************************************************
** ��������: ISRTimer2
** ��������: ��ʱ��2�жϴ�����������AD��DAת�����˲��ִ������������ܣ�����У׼PID���ƣ��Լ���λ�����ݲɼ��Ӷ���������ֵ��
** �䡡��: ��
** �䡡�� : ��
** ȫ�ֱ���: precmd_change��PrePID
** ����ģ��: ��
** ״     ̬: PID���ڴ���û��д��ѹ����������ֵû�и��¡���������λ����������ֵ���ܴ���û�����
** ������: �˿���
** �ա���: 20151108
**-------------------------------------------------------------------------------------------------------
** �޸���:
** �ա���:
**-------------------------------------------------------------------------------------------------------
********************************************************************************************************/
interrupt void ISRTimer2(void)
{
	//
	char i;
	Uint16 DA_Convert;
	static Uint16 Pre_AD_Old, Flow_AD_Old;
	DINT;
	PieCtrlRegs.PIEACK.all = PIEACK_GROUP1;
	CpuTimer2Regs.TCR.bit.TIF=1;	//��ʱ����ָ��ʱ�䣬�ñ�־λ
	CpuTimer2Regs.TCR.bit.TRB=1;	//����Timer1�Ķ�ʱ����
#ifdef DEBUG
	LED4=~LED4;
#endif
	CLEAR_ADCON;   //����ת���ź�
	DELAY_US(1); //�����ʵ��ĵ�ƽ��ʱ
	AD_CONVST;
	DELAY_US(1);
	while(AD_BUSY) //�ȴ�ת������
	{
	}
	for(i=0;i<4;i++)
	{
		ad[i]=AD7606_BASIC; //��ȡ6·ADͨ������
	}
	DA_Convert = (ad[0]+5)*2;//ad[2]����ѹ�����ڣ�ad[3]�����������ڣ�
	spi_xmit(DA_Convert);
	if(ad[2]>50000)
		ad[2]=0;
	if(ad[3]>50000)
		ad[3]=0;
	if(abs(Pre_AD_Old-ad[2])>5)
	{
		PrePID.SetCmd=(int)((double)(ad[2]/32768.0)*15252010)+1525201;//�ڴ˴���ѹ������ֵ�޸ģ���������Ϊ��ť��ת
		//CMD_Change[2]=0xAA;
		//CMD_Change[3]=0;
		precmd_change[0]=MI_UINT32(ad[2]);//�˴�����λ��ADת��ֵ���浽ȫ�ֱ�����ȥ�����ڶ�ʱ��0�н��������͵���λ��������û�н��е�λ�任����֤�������ܹ�����ת��
		precmd_change[1]=LO_UINT32(ad[2]);
		//CMD_Change[6]=XorCheckSum(CMD_Change, 6);
		//SendData(CMD_Change);
	}
	if(abs(Flow_AD_Old-ad[3])>5)//the num of 300 needs to be determined
	{
		//CMD_Change[2]=0xAA;
		//CMD_Change[3]=0;
		//CMD_Change[4]=MI_UINT32(ad[2]);
		//CMD_Change[5]=LO_UINT32(ad[2]);
		//CMD_Change[6]=XorCheckSum(CMD_Change, 6);
		//SendData(CMD_Change);
	}
	Pre_AD_Old = ad[2];
	Flow_AD_Old = ad[3];
	//testSample1[sampleCount]=ad[0];//�����������
	//testSample2[sampleCount]=ad[1];
	//testSample3[sampleCount]=ad[2];
	//testSample4[sampleCount]=ad[3];
			//testSample5[sampleCount]=ad[4];
			//testSample6[sampleCount]=ad[5];
	//sampleCount++;
	//if(sampleCount >= 255) sampleCount=0;
	EINT;
}

/*
 * �˶�Ϊ�������ö�
 */
void INTConfig(void)
{
	CfgLed();
	CfgTimer0();//�㷨������жϺ�����
	CfgTimer1();
	CfgTimer2();
	CfgScia();//����ָ��������жϺ�����
	CfgScib();
	CfgScic();//����λ����������������жϺ�����
	CfgCap();//���������ݲɼ�������жϺ�����
}

//�ж��ߵ����ú���
void CfgScia(void)
{
	PieCtrlRegs.PIECTRL.bit.ENPIE = 1;   // Enable the PIE block
	PieCtrlRegs.PIEIER9.bit.INTx1=1;     // PIE Group 9, int1
	PieCtrlRegs.PIEIER9.bit.INTx2=1;     // PIE Group 9, INT2
	IER |= M_INT9;	// Enable CPU INT
	EINT;
}

void CfgScib(void)
{
	PieCtrlRegs.PIEIER9.bit.INTx3=1;     // PIE Group 8, int5
	PieCtrlRegs.PIEIER9.bit.INTx4=1;     // PIE Group 8, INT6
	IER |= M_INT9;	// Enable CPU INT
}

void CfgScic(void)
{
	DINT;
	PieCtrlRegs.PIECTRL.bit.ENPIE = 1;   // Enable the PIE block
	PieCtrlRegs.PIEIER8.bit.INTx5=1;     // PIE Group 8, int5
	PieCtrlRegs.PIEIER8.bit.INTx6=1;     // PIE Group 8, INT6
	IER |= M_INT8;	// Enable CPU INT
	EINT;
}

void CfgCap(void)
{
	DINT;
	PieCtrlRegs.PIEIER4.bit.INTx1 = 1;
	PieCtrlRegs.PIEIER4.bit.INTx2 = 1;
	IER |= M_INT4;
	EINT;
}

void CfgTimer0(void)
{
	DINT;
	PieCtrlRegs.PIECTRL.bit.ENPIE = 1;   // Enable the PIE block
	PieCtrlRegs.PIEIER1.bit.INTx7 = 1;
	IER |= M_INT1;
	EINT;
}

void CfgTimer1(void)
{
	DINT;
	IER |= M_INT13;
	CpuTimer1Regs.TCR.all=0x4001;
	EINT;
}

void CfgTimer2(void)
{
	DINT;
	IER |= M_INT14;
	CpuTimer2Regs.TCR.all=0x4001;
	EINT;
}

//
void CfgLed(void)
{
   EALLOW;
   GpioCtrlRegs.GPAMUX1.bit.GPIO0 = 0; //GPIO0����ΪGPIO����
   GpioCtrlRegs.GPADIR.bit.GPIO0 = 1;  // GPIO0����Ϊ���
   GpioCtrlRegs.GPAMUX1.bit.GPIO1 = 0; // GPIO1 = GPIO1
   GpioCtrlRegs.GPADIR.bit.GPIO1 = 1;
   GpioCtrlRegs.GPAMUX1.bit.GPIO2 = 0; //
   GpioCtrlRegs.GPADIR.bit.GPIO2 = 1;
   GpioCtrlRegs.GPAMUX1.bit.GPIO3 = 0; //
   GpioCtrlRegs.GPADIR.bit.GPIO3 = 1;
   GpioCtrlRegs.GPAMUX1.bit.GPIO4 = 0; //
   GpioCtrlRegs.GPADIR.bit.GPIO4 = 1;
   GpioCtrlRegs.GPAMUX1.bit.GPIO6 = 0; //
   GpioCtrlRegs.GPADIR.bit.GPIO6 = 1;
   EDIS;
}

/*
 * �˶�Ϊ�Զ����
 */
//����A���ͺ���
void SendData(unsigned char *buf)
{
	unsigned char i;
	for(i=0;i<8;i++)
	{
		sdataA[i]=(*(buf++));
	}
	//SciaRegs.SCIFFTX.bit.TXFFINTCLR=1;  // Clear Interrupt flag
	ScibRegs.SCIFFTX.bit.TXFFINTCLR=1;  // Clear Interrupt flag
}

//����C��������
void SendCmd(unsigned char *buf)
{
	unsigned char i;
	for(i=0;i<12;i++)
	{
		sdataC[i]=(*(buf++));
	}
	ScicRegs.SCIFFTX.bit.TXFFINTCLR=1;  // Clear Interrupt flag
}

unsigned char XorCheckSum(unsigned char * pBuf, char len)
{
	unsigned char i;
	unsigned char byRet=0;
	if(len == 0)
		return byRet;
	else
		byRet = pBuf[0];
	for(i=1;i<len;i++)
		byRet = byRet^pBuf[i];

	return byRet;
}
//===========================================================================
// No more.
//===========================================================================

