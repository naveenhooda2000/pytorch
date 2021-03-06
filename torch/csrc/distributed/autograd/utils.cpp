#include <torch/csrc/autograd/functions/utils.h>
#include <torch/csrc/distributed/autograd/context/dist_autograd_container.h>
#include <torch/csrc/distributed/autograd/functions/recvrpc_backward.h>
#include <torch/csrc/distributed/autograd/functions/sendrpc_backward.h>
#include <torch/csrc/distributed/autograd/utils.h>
#include <torch/csrc/distributed/rpc/rpc_agent.h>

namespace torch {
namespace distributed {
namespace autograd {

using torch::distributed::rpc::Message;

void addSendRpcBackward(
    DistAutogradContext& autogradContext,
    const AutogradMetadata& autogradMetadata,
    std::vector<torch::Tensor>& tensors) {
  // Attach the appropriate autograd edges.
  if (torch::autograd::compute_requires_grad(tensors)) {
    auto grad_fn = std::make_shared<SendRpcBackward>();
    grad_fn->set_next_edges(torch::autograd::collect_next_edges(tensors));

    // Add the appropriate input metadata for the grad_fn.
    for (const auto& tensor : tensors) {
      grad_fn->add_input_metadata(tensor);
    }

    // Record the send autograd function in our current context.
    autogradContext.addSendFunction(
        grad_fn, autogradMetadata.autogradMessageId);
  }
}

DistAutogradContext* addRecvRpcBackward(
    const AutogradMetadata& autogradMetadata,
    std::vector<torch::Tensor>& tensors,
    rpc::worker_id_t fromWorkerId) {
  if (torch::autograd::compute_requires_grad(tensors)) {
    // Initialize autograd context if necessary.
    auto& autogradContainer = DistAutogradContainer::getInstance();
    DistAutogradContext& autogradContext = autogradContainer.getOrCreateContext(
        autogradMetadata.autogradContextId);

    // Attach the tensors as inputs to the autograd function.
    auto grad_fn = std::make_shared<RecvRpcBackward>(
        autogradMetadata, autogradContext, fromWorkerId);
    for (auto& tensor : tensors) {
      torch::autograd::set_history(tensor, grad_fn);
    }

    // Now update the autograd context with the necessary information.
    autogradContext.addRecvFunction(
        grad_fn, autogradMetadata.autogradMessageId);
    return &autogradContext;
  }
  return nullptr;
}

} // namespace autograd
} // namespace distributed
} // namespace torch
