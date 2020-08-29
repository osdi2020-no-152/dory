#include <cstring>
#include <stdexcept>

#include "rc.hpp"
#include "wr-builder.hpp"

// namespace dory {
//   /**
//    * Handle the completion status of a WC
//    */
//   static int handle_work_completion(struct ibv_wc *wc) {
//       int rc = 0;

//     /* Verify completion status */
//     switch (wc->status) {
//       case IBV_WC_SUCCESS:
//           /* IBV_WC_SUCCESS: Operation completed successfully */
//           rc = WC_SUCCESS;
//           break;
//       case IBV_WC_REM_ACCESS_ERR:  //  Remote Access Error
//           rc = WC_EXPECTED_ERROR;
//           fprintf(stderr, "Expected error: WC has status %s (%d) \n",
//                   ibv_wc_status_str(wc->status), wc->status);
//           break;

//       case IBV_WC_LOC_LEN_ERR:         //  Local Length Error
//       case IBV_WC_LOC_QP_OP_ERR:       //  Local QP Operation Error
//       case IBV_WC_LOC_EEC_OP_ERR:      //  Local EE Context Operation Error
//       case IBV_WC_LOC_PROT_ERR:        //  Local Protection Error
//       case IBV_WC_MW_BIND_ERR:         //  Memory Window Binding Error
//       case IBV_WC_LOC_ACCESS_ERR:      //  Local Access Error
//       case IBV_WC_RNR_RETRY_EXC_ERR:   // RNR Retry Counter Exceeded
//       case IBV_WC_LOC_RDD_VIOL_ERR:    // Local RDD Violation Error
//       case IBV_WC_REM_INV_RD_REQ_ERR:  // Remote Invalid RD Request
//       case IBV_WC_REM_ABORT_ERR:       // Remote Aborted Error
//       case IBV_WC_INV_EECN_ERR:        // Invalid EE Context Number
//       case IBV_WC_INV_EEC_STATE_ERR:   // Invalid EE Context State Error
//       case IBV_WC_WR_FLUSH_ERR:
//           /* Work Request Flushed Error: A Work Request was in
//           process or outstanding when the QP transitioned into the
//           Error State. */
//       case IBV_WC_BAD_RESP_ERR:
//           /* Bad Response Error - an unexpected transport layer
//           opcode was returned by the responder. */
//       case IBV_WC_REM_INV_REQ_ERR:
//           /* Remote Invalid Request Error: The responder detected an
//           invalid message on the channel. Possible causes include the
//           operation is not supported by this receive queue, insufficient
//           buffering to receive a new RDMA or Atomic Operation request,
//           or the length specified in an RDMA request is greater than
//           2^{31} bytes. Relevant for RC QPs. */
//       case IBV_WC_REM_OP_ERR:
//           /* Remote Operation Error: the operation could not be
//           completed successfully by the responder. Possible causes
//           include a responder QP related error that prevented the
//           responder from completing the request or a malformed WQE on
//           the Receive Queue. Relevant for RC QPs. */
//       case IBV_WC_RETRY_EXC_ERR:
//           /* Transport Retry Counter Exceeded: The local transport
//           timeout retry counter was exceeded while trying to send this
//           message. This means that the remote side didn’t send any Ack
//           or Nack. If this happens when sending the first message,
//           usually this mean that the connection attributes are wrong or
//           the remote side isn’t in a state that it can respond to messages.
//           If this happens after sending the first message, usually it
//           means that the remote QP isn’t available anymore. */
//           /* REMOTE SIDE IS DOWN */
//       case IBV_WC_FATAL_ERR:
//           /* Fatal Error - WTF */
//       case IBV_WC_RESP_TIMEOUT_ERR:
//           /* Response Timeout Error */
//       case IBV_WC_GENERAL_ERR:
//           /* General Error: other error which isn’t one of the above errors.
//             */

//           rc = WC_UNEXPECTED_ERROR;
//           fprintf(stderr, "Unexpected error: WC has status %s (%d) \n",
//                   ibv_wc_status_str(wc->status), wc->status);
//           break;
//     }

//     return rc;
//   }
// }

namespace dory {
ReliableConnection::ReliableConnection(ControlBlock &cb)
    : cb{cb}, pd{nullptr}, LOGGER_INIT(logger, "RC") {
  memset(&create_attr, 0, sizeof(struct ibv_qp_init_attr));
  create_attr.qp_type = IBV_QPT_RC;
  create_attr.cap.max_send_wr = WRDepth;
  create_attr.cap.max_recv_wr = WRDepth;
  create_attr.cap.max_send_sge = SGEDepth;
  create_attr.cap.max_recv_sge = SGEDepth;
  create_attr.cap.max_inline_data = MaxInlining;
}

void ReliableConnection::bindToPD(std::string pd_name) {
  pd = cb.pd(pd_name).get();
}

void ReliableConnection::bindToMR(std::string mr_name) { mr = cb.mr(mr_name); }

// TODO(Kristian): creation of qp should be rather separated?
void ReliableConnection::associateWithCQ(std::string send_cp_name,
                                         std::string recv_cp_name) {
  create_attr.send_cq = cb.cq(send_cp_name).get();
  create_attr.recv_cq = cb.cq(recv_cp_name).get();

  auto qp = ibv_create_qp(pd, &create_attr);

  if (qp == nullptr) {
    throw std::runtime_error("Could not create the queue pair");
  }

  uniq_qp = deleted_unique_ptr<struct ibv_qp>(qp, [](struct ibv_qp *qp) {
    auto ret = ibv_destroy_qp(qp);
    if (ret != 0) {
      throw std::runtime_error("Could not query device: " +
                               std::string(std::strerror(errno)));
    }
  });
}

void ReliableConnection::reset() {
  struct ibv_qp_attr attr;
  memset(&attr, 0, sizeof(attr));

  attr.qp_state = IBV_QPS_RESET;

  auto ret = ibv_modify_qp(uniq_qp.get(), &attr, IBV_QP_STATE);

  if (ret != 0) {
    throw std::runtime_error("Could not modify QP to RESET: " +
                             std::string(std::strerror(errno)));
  }
}

void ReliableConnection::init(ControlBlock::MemoryRights rights) {
  struct ibv_qp_attr init_attr;
  memset(&init_attr, 0, sizeof(struct ibv_qp_attr));
  init_attr.qp_state = IBV_QPS_INIT;
  init_attr.pkey_index = 0;
  init_attr.port_num = static_cast<uint8_t>(cb.port());
  init_attr.qp_access_flags = rights;

  auto ret = ibv_modify_qp(
      uniq_qp.get(), &init_attr,
      IBV_QP_STATE | IBV_QP_PKEY_INDEX | IBV_QP_PORT | IBV_QP_ACCESS_FLAGS);

  if (ret != 0) {
    throw std::runtime_error("Failed to bring conn QP to INIT: " +
                             std::string(std::strerror(errno)));
  }

  init_rights = rights;
}

void ReliableConnection::reinit() { init(init_rights); }

void ReliableConnection::connect(RemoteConnection &rc) {
  memset(&conn_attr, 0, sizeof(struct ibv_qp_attr));
  conn_attr.qp_state = IBV_QPS_RTR;
  conn_attr.path_mtu = IBV_MTU_4096;
  conn_attr.rq_psn = DefaultPSN;

  conn_attr.ah_attr.is_global = 0;
  conn_attr.ah_attr.sl = 0;  // TODO: Igor has it to 1
  conn_attr.ah_attr.src_path_bits = 0;
  conn_attr.ah_attr.port_num = static_cast<uint8_t>(cb.port());

  conn_attr.dest_qp_num = rc.rci.qpn;
  conn_attr.ah_attr.dlid = rc.rci.lid;

  conn_attr.max_dest_rd_atomic = 16;
  conn_attr.min_rnr_timer = 12;

  int rtr_flags = IBV_QP_STATE | IBV_QP_AV | IBV_QP_PATH_MTU | IBV_QP_DEST_QPN |
                  IBV_QP_RQ_PSN | IBV_QP_MAX_DEST_RD_ATOMIC |
                  IBV_QP_MIN_RNR_TIMER;

  auto ret = ibv_modify_qp(uniq_qp.get(), &conn_attr, rtr_flags);
  if (ret != 0) {
    throw std::runtime_error("Failed to bring conn QP to RTR: " +
                             std::string(std::strerror(errno)));
  }

  memset(&conn_attr, 0, sizeof(struct ibv_qp_attr));
  conn_attr.qp_state = IBV_QPS_RTS;
  conn_attr.sq_psn = DefaultPSN;

  conn_attr.timeout = 14;
  conn_attr.retry_cnt = 7;
  conn_attr.rnr_retry = 7;
  conn_attr.max_rd_atomic = 16;
  conn_attr.max_dest_rd_atomic = 16;

  int rts_flags = IBV_QP_STATE | IBV_QP_SQ_PSN | IBV_QP_TIMEOUT |
                  IBV_QP_RETRY_CNT | IBV_QP_RNR_RETRY | IBV_QP_MAX_QP_RD_ATOMIC;

  ret = ibv_modify_qp(uniq_qp.get(), &conn_attr, rts_flags);
  if (ret != 0) {
    throw std::runtime_error("Failed to bring conn QP to RTS: " +
                             std::string(std::strerror(errno)));
  }

  rconn = rc;

  // This has to happen here, because when the object is copied, the pointer
  // breaks!
  struct ibv_send_wr *wr_ = reinterpret_cast<ibv_send_wr *>(aligned_alloc(64, roundUp(sizeof(ibv_send_wr), 64) + sizeof(ibv_sge)));
  struct ibv_sge *sg_ =
      reinterpret_cast<ibv_sge *>(
        reinterpret_cast<char *>(wr_) + roundUp(sizeof(ibv_send_wr), 64));

  memset(sg_, 0, sizeof(*sg_));
  memset(wr_, 0, sizeof(*wr_));

  wr_->sg_list = sg_;
  wr_->num_sge = 1;
  wr_->send_flags |= IBV_SEND_SIGNALED;

  sg_->lkey = mr.lkey;
  wr_->wr.rdma.rkey = rconn.rci.rkey;

  wr_cached = deleted_unique_ptr<struct ibv_send_wr>(wr_, wr_deleter);
}

bool ReliableConnection::needsReset() {
  struct ibv_qp_attr attr;
  struct ibv_qp_init_attr init_attr;

  if (ibv_query_qp(uniq_qp.get(), &attr, IBV_QP_STATE, &init_attr)) {
    throw std::runtime_error("Failed to query QP state: " +
                             std::string(std::strerror(errno)));
  }

  return attr.qp_state == IBV_QPS_RTS;
}

bool ReliableConnection::changeRights(ControlBlock::MemoryRights rights) {
  struct ibv_qp_attr attr;
  memset(&attr, 0, sizeof(attr));

  attr.qp_access_flags = static_cast<int>(rights);

  auto ret = ibv_modify_qp(uniq_qp.get(), &attr, IBV_QP_ACCESS_FLAGS);
  return ret == 0;
}

bool ReliableConnection::changeRightsIfNeeded(
    ControlBlock::MemoryRights rights) {
  auto converted_rights = static_cast<int>(rights);

  struct ibv_qp_attr attr;
  struct ibv_qp_init_attr init_attr;

  if (ibv_query_qp(uniq_qp.get(), &attr, IBV_QP_STATE | IBV_QP_ACCESS_FLAGS,
                   &init_attr)) {
    throw std::runtime_error("Failed to query QP state: " +
                             std::string(std::strerror(errno)));
  }

  if (attr.qp_state != IBV_QPS_RTS) {
    return false;
  }

  if (attr.qp_access_flags == converted_rights) {
    return true;
  }

  return changeRights(rights);
}

bool ReliableConnection::post_send(ibv_send_wr &wr) {
  struct ibv_send_wr *bad_wr = nullptr;

  auto ret = ibv_post_send(uniq_qp.get(), &wr, &bad_wr);

  if (bad_wr != nullptr) {
    LOGGER_DEBUG(logger, "Got bad wr with id: {}", bad_wr->wr_id);
    return false;
    // throw std::runtime_error("Error encountered during posting in some work
    // request");
  }

  if (ret != 0) {
    throw std::runtime_error("Error due to driver misuse during posting: " +
                             std::string(std::strerror(errno)));
  }

  return true;
}

bool ReliableConnection::postSendSingleCached(RdmaReq req, uint64_t req_id,
                                              void *buf, uint32_t len,
                                              uintptr_t remote_addr) {
  wr_cached->sg_list->addr = reinterpret_cast<uintptr_t>(buf);
  wr_cached->sg_list->length = len;

  wr_cached->wr_id = req_id;
  wr_cached->opcode = static_cast<enum ibv_wr_opcode>(req);

  if (wr_cached->opcode == IBV_WR_RDMA_WRITE && len <= MaxInlining) {
    wr_cached->send_flags |= IBV_SEND_INLINE;
  } else {
    wr_cached->send_flags &= ~static_cast<unsigned int>(IBV_SEND_INLINE);
  }

  wr_cached->wr.rdma.remote_addr = remote_addr;

  struct ibv_send_wr *bad_wr = nullptr;
  auto ret = ibv_post_send(uniq_qp.get(), wr_cached.get(), &bad_wr);

  if (bad_wr != nullptr) {
    LOGGER_DEBUG(logger, "Got bad wr with id: {}", bad_wr->wr_id);
    return false;
    // throw std::runtime_error(
    //     "Error encountered during posting in some work request");
  }

  if (ret != 0) {
    throw std::runtime_error("Error due to driver misuse during posting: " +
                             std::string(std::strerror(errno)));
  }

  return true;
}

bool ReliableConnection::postSendSingle(RdmaReq req, uint64_t req_id, void *buf,
                                        uint32_t len, uint32_t lkey,
                                        uintptr_t remote_addr) {
  // TODO(Kristian): if not used concurrently, we could reuse the same wr
  struct ibv_send_wr wr;
  struct ibv_sge sg;

  SendWrBuilder()
      .req(req)
      .signaled(true)
      .req_id(req_id)
      .buf(buf)
      .len(len)
      .lkey(lkey)
      .remote_addr(remote_addr)
      .rkey(rconn.rci.rkey)
      .build(wr, sg);

  return post_send(wr);
}

void ReliableConnection::reconnect() { connect(rconn); }

bool ReliableConnection::pollCqIsOK(CQ cq,
                                    std::vector<struct ibv_wc> &entries) {
  int num = 0;

  switch (cq) {
    case RecvCQ:
      num = ibv_poll_cq(create_attr.recv_cq, static_cast<int>(entries.size()),
                        &entries[0]);
      break;
    case SendCQ:
      num = ibv_poll_cq(create_attr.send_cq, static_cast<int>(entries.size()),
                        &entries[0]);
      break;
    default:
      throw std::runtime_error("Invalid CQ");
  }

  if (num >= 0) {
    entries.erase(entries.begin() + num, entries.end());
    return true;
  } else {
    return false;
  }
}

RemoteConnection ReliableConnection::remoteInfo() const {
  RemoteConnection rc(static_cast<uint16_t>(cb.lid()), uniq_qp->qp_num, mr.addr,
                      mr.size, mr.rkey);
  return rc;
}

void ReliableConnection::query_qp(ibv_qp_attr &qp_attr,
                                  ibv_qp_init_attr &init_attr,
                                  int attr_mask) const {
  ibv_query_qp(uniq_qp.get(), &qp_attr, attr_mask, &init_attr);
}

}  // namespace dory
