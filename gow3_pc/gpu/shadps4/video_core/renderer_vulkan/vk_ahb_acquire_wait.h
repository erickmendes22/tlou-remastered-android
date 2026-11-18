// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

namespace Vulkan {
// An AHardwareBuffer has no release fence on its first use, or after a CPU wait.
// vkQueueSubmit must never receive VK_NULL_HANDLE in pWaitSemaphores.
template <typename Submission, typename Semaphore>
void AddImageAcquiredWait(Submission& submission, Semaphore acquired) {
    if (acquired) {
        submission.AddWait(acquired);
    }
}
} // namespace Vulkan
