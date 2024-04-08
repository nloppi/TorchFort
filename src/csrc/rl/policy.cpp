/*
 * SPDX-FileCopyrightText: Copyright (c) 2023-2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <algorithm>
#include <torch/torch.h>

#include "internal/exceptions.h"
#include "internal/rl/distributions.h"
#include "internal/rl/policy.h"

namespace torchfort {

namespace rl {

ACPolicy::ACPolicy(std::shared_ptr<ModelWrapper> p_mu_log_sigma)
    : p_mu_log_sigma_(p_mu_log_sigma), log_sigma_min_(-20.), log_sigma_max_(2.) {}

std::vector<torch::Tensor> ACPolicy::parameters() const {
  std::vector<torch::Tensor> result = p_mu_log_sigma_->parameters();
  return result;
}

void ACPolicy::train() { p_mu_log_sigma_->train(); }

void ACPolicy::eval() { p_mu_log_sigma_->eval(); }

void ACPolicy::to(torch::Device device, bool non_blocking) { p_mu_log_sigma_->to(device, non_blocking); }

void ACPolicy::save(const std::string& fname) const { p_mu_log_sigma_->save(fname); }

void ACPolicy::load(const std::string& fname) { p_mu_log_sigma_->load(fname); }

std::tuple<torch::Tensor, torch::Tensor> ACPolicy::forwardNoise(torch::Tensor state) {
  // predict mu
  auto fwd = p_mu_log_sigma_->forward(std::vector<torch::Tensor>{state});
  auto& action_mu = fwd[0];
  auto& action_log_sigma = fwd[1];
  // predict sigma
  auto action_sigma = torch::exp(torch::clamp(action_log_sigma, log_sigma_min_, log_sigma_max_));

  // create distribution
  auto pi_dist = NormalDistribution(action_mu, action_sigma);

  // sample action and compute log prob
  // do not squash yet
  auto action = pi_dist.rsample();
  auto action_log_prob = torch::sum(torch::flatten(pi_dist.log_prob(action), 1), 1, true);

  // account for squashing
  action_log_prob =
      action_log_prob -
      torch::sum(torch::flatten(2. * (std::log(2.) - action - torch::softplus(-2. * action)), 1), 1, true);
  action = torch::tanh(action);

  return std::make_tuple(action, action_log_prob);
}

torch::Tensor ACPolicy::forwardDeterministic(torch::Tensor state) {
  // predict mu is the only part
  auto action = torch::tanh(p_mu_log_sigma_->forward(std::vector<torch::Tensor>{state})[0]);

  return action;
}

} // namespace rl

} // namespace torchfort
