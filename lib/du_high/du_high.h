
#ifndef SRSGNB_DU_HIGH_H
#define SRSGNB_DU_HIGH_H

#include "adapters.h"
#include "srsgnb/du_manager/du_manager.h"
#include "srsgnb/f1_interface/f1ap_du.h"
#include "srsgnb/mac/mac.h"
#include "srsgnb/ran/du_l2_ul_executor_mapper.h"
#include "srsgnb/rlc/rlc.h"
#include "srsgnb/support/executors/task_executor.h"
#include "srsgnb/support/executors/task_worker.h"
#include <memory>

namespace srsgnb {

class du_high
{
public:
  du_high(f1c_du_gateway& gw);
  ~du_high();

  void start();
  void stop();

  void push_pusch(mac_rx_data_indication pdu);

private:
  std::unique_ptr<du_manager_interface> du_manager;
  std::unique_ptr<f1ap_du_interface>    f1ap;
  std::unique_ptr<mac_interface>        mac;

  rlc_ul_sdu_adapter             rlc_sdu_notifier;
  f1ap_du_rlc_connector          f1ap_pdu_adapter;
  du_manager_mac_event_indicator mac_ev_notifier;

  std::vector<std::unique_ptr<task_worker> >   workers;
  std::unique_ptr<task_executor>               ctrl_exec;
  std::vector<std::unique_ptr<task_executor> > dl_execs;
  std::unique_ptr<du_l2_ul_executor_mapper>    ul_exec_mapper;
};

} // namespace srsgnb

#endif // SRSGNB_DU_HIGH_H
