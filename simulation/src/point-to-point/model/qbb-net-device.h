/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
* Copyright (c) 2006 Georgia Tech Research Corporation, INRIA
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 as
* published by the Free Software Foundation;
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*
* Author: Yibo Zhu <yibzh@microsoft.com>
*/
#ifndef QBB_NET_DEVICE_H
#define QBB_NET_DEVICE_H

#include "ns3/point-to-point-net-device.h"
#include "ns3/qbb-channel.h"
#include "ns3/event-id.h"
#include "ns3/broadcom-egress-queue.h"
#include "ns3/ipv4.h"
#include "ns3/ipv4-header.h"
#include "ns3/udp-header.h"
#include "ns3/rdma-queue-pair.h"
#include <vector>
#include <map>
#include <ns3/rdma.h>

namespace ns3 {

class QbbNetDevice; // added: forward declaration for qb_dev pointer

class RdmaEgressQueue : public Object{
public:
	static const uint32_t qCnt = 8;
	static uint32_t ack_q_idx;
	static uint32_t tcpip_q_idx; // added: TCP/IP queue index
	int m_qlast;
	uint32_t m_rrlast;
	Ptr<SimpleDropTailQueue> m_ackQ; // highest priority queue
	Ptr<RdmaQueuePairGroup> m_qpGrp; // queue pairs

	// callback for get next packet
	typedef Callback<Ptr<Packet>, Ptr<RdmaQueuePair> > RdmaGetNxtPkt;
	RdmaGetNxtPkt m_rdmaGetNxtPkt;

	static TypeId GetTypeId (void);
	RdmaEgressQueue();
	Ptr<Packet> DequeueQindex(int qIndex);
	int GetNextQindex(bool paused[]);
	int GetLastQueue();
	uint32_t GetNBytes(uint32_t qIndex);
	uint32_t GetFlowCount(void);
	Ptr<RdmaQueuePair> GetQp(uint32_t i);
	void RecoverQueue(uint32_t i);
	void EnqueueHighPrioQ(Ptr<Packet> p);
	void CleanHighPrio(TracedCallback<Ptr<const Packet>, uint32_t> dropCb);

	TracedCallback<Ptr<const Packet>, uint32_t> m_traceRdmaEnqueue;
	TracedCallback<Ptr<const Packet>, uint32_t> m_traceRdmaDequeue;

	// added: fields for TCP/IP scheduling support
	Ptr<QbbNetDevice> qb_dev;
	bool dummy_paused[8];
	uint64_t hostDequeueIndex;
};

/**
 * \class QbbNetDevice
 * \brief A Device for a IEEE 802.1Qbb Network Link.
 */
class QbbNetDevice : public PointToPointNetDevice 
{
public:
  static const uint32_t qCnt = 8;	// Number of queues/priorities used

  static TypeId GetTypeId (void);

  QbbNetDevice ();
  virtual ~QbbNetDevice ();

  virtual void Receive (Ptr<Packet> p);

  virtual bool Send(Ptr<Packet> packet, const Address &dest, uint16_t protocolNumber);
  virtual bool SwitchSend (uint32_t qIndex, Ptr<Packet> packet, CustomHeader &ch);

  // added: TCP/IP support methods
  Address GetRemote (void) const;
  virtual void SetReceiveCallback (NetDevice::ReceiveCallback cb);
  DataRate GetDataRate();

  void ConnectWithoutContext(const CallbackBase& callback);
  void DisconnectWithoutContext(const CallbackBase& callback);

  bool Attach (Ptr<QbbChannel> ch);

  virtual Ptr<Channel> GetChannel (void) const;

  void SetQueue (Ptr<BEgressQueue> q);
  Ptr<BEgressQueue> GetQueue ();
  virtual bool IsQbb(void) const;
  void NewQp(Ptr<RdmaQueuePair> qp);
  void ReassignedQp(Ptr<RdmaQueuePair> qp);
  void TriggerTransmit();
  void SwitchAsHostSend(void); // Switch as a Host to send packet, such as reply ack with NVLS ON
  void SwitchDequeueAndTransmit(void); // Switch transfer to other node
  bool SwitchAsHostTransmitStart(Ptr<Packet> p);
  void SwitchAsHostTransmitComplete(void);
   
  void SendPfc(uint32_t qIndex, uint32_t type); // type: 0 = pause, 1 = resume
  Ptr<Packet> NICSendPfc(uint32_t qIndex, uint32_t type);

  TracedCallback<Ptr<const Packet>, uint32_t> m_traceEnqueue;
  TracedCallback<Ptr<const Packet>, uint32_t> m_traceDequeue;
  TracedCallback<Ptr<const Packet>, uint32_t> m_traceDrop;
  TracedCallback<uint32_t> m_tracePfc; // 0: resume, 1: pause

  // added: byte counting metrics for TCP/IP monitoring
  uint64_t getTxBytes(){
      uint64_t temp=numTxBytes;
      numTxBytes=0;
      return temp;
  }
  uint64_t numTxBytes=0;
  uint64_t numTxBytesLast=0;
  uint64_t totalBytesSent=0;

  uint64_t getNumTxBytes(){
      uint64_t temp;
      temp = totalBytesSent-numTxBytesLast;
      numTxBytesLast=totalBytesSent;
      return temp;
  }

  uint64_t numRxBytes=0;
  uint64_t numRxBytesLast=0;
  uint64_t totalBytesRcvd=0;
	
  uint64_t getNumRxBytes(){
      uint64_t temp;
      temp = totalBytesRcvd-numRxBytesLast;
      numRxBytesLast=totalBytesRcvd;
      return temp;
  }

protected:

  bool TransmitStart (Ptr<Packet> p);
  
  virtual void DoDispose(void);

  virtual void TransmitComplete(void);

  virtual void DequeueAndTransmit(void);

  virtual void Resume(unsigned qIndex);

  // added: header processing for TCP/IP
  bool ProcessHeader (Ptr<Packet> p, uint16_t& param);
  static uint16_t PppToEther (uint16_t proto);
  static uint16_t EtherToPpp (uint16_t proto);

  Ptr<BEgressQueue> m_queue;
  Ptr<QbbChannel> m_channel;
  
  //pfc
  bool m_qbbEnabled;	
  bool m_qcnEnabled;
  bool m_dynamicth;
  uint32_t m_pausetime;	
  bool m_paused[qCnt];	
  bool dummy_paused[qCnt]; // added: dummy paused array

  uint32_t nvls_enable;

  //qcn
  EventId  m_nextSend;		

  struct ECNAccount{
	  Ipv4Address source;
	  uint32_t qIndex;
	  uint32_t port;
	  uint8_t ecnbits;
	  uint16_t qfb;
	  uint16_t total;
  };

  std::vector<ECNAccount> *m_ecn_source;

public:
  Ptr<RdmaEgressQueue> m_rdmaEQ;
  void RdmaEnqueueHighPrioQ(Ptr<Packet> p);
  void SendCallback(Ptr<Packet> Packet);
  
  typedef Callback<int, Ptr<Packet>, CustomHeader &> RdmaSentCb;
  RdmaSentCb m_rdmaSentCb;
  
  typedef Callback<int, Ptr<Packet>, CustomHeader&> RdmaReceiveCb;
  RdmaReceiveCb m_rdmaReceiveCb;
  
  typedef Callback<void, Ptr<QbbNetDevice> > RdmaLinkDownCb;
  RdmaLinkDownCb m_rdmaLinkDownCb;
  
  typedef Callback<void, Ptr<RdmaQueuePair>, Ptr<Packet>, Time> RdmaPktSent;
  RdmaPktSent m_rdmaPktSent;
  Callback<void, uint32_t, uint64_t> m_rdmaUpdateTxBytes;

  Ptr<RdmaEgressQueue> GetRdmaQueue();
  void TakeDown(); 
  void UpdateNextAvail(Time t);

  TracedCallback<Ptr<const Packet>, Ptr<RdmaQueuePair> > m_traceQpDequeue; 
};

} // namespace ns3

#endif // QBB_NET_DEVICE_H