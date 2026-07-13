#ifndef _FREERTOS_CONFIG_H
#define _FREERTOS_CONFIG_H

#define configUSE_PREEMPTION            1
#define configUSE_PORT_OPTIMISED_TASK_SELECTION 0
#define configUSE_TICKLESS_IDLE         0
#define configUSE_TIMERS                1
#define configCPU_CLOCK_HZ              133000000UL
#define configTICK_RATE_HZ              1000
#define configMAX_PRIORITIES            8
#define configMINIMAL_STACK_SIZE        128
#define configTOTAL_HEAP_SIZE           (128 * 1024)
#define configMAX_TASK_NAME_LEN         16
#define configUSE_16_BIT_TICKS          0
#define configIDLE_SHOULD_YIELD         1
#define configUSE_TASK_NOTIFICATIONS    1
#define configTASK_NOTIFICATION_ARRAY_ENTRIES 3
#define configUSE_MUTEXES               1
#define configUSE_RECURSIVE_MUTEXES     0
#define configUSE_COUNTING_SEMAPHORES   0
#define configQUEUE_REGISTRY_SIZE       8
#define configUSE_QUEUE_SETS            0
#define configUSE_NEWLIB_REENTRANT      0
#define configENABLE_BACKWARD_COMPATIBILITY 0
#define configSTACK_ALLOCATION_FROM_SEPARATE_HEAP 0
#define configCHECK_FOR_STACK_OVERFLOW  2
#define configUSE_TIME_SLICING          1
#define configUSE_MINIMAL_IDLE_HOOK     1
#define configUSE_DAEMON_TASK_STARTUP_HOOK 0
#define configUSE_IDLE_HOOK             0
#define configUSE_TICK_HOOK             0
#define configUSE_MALLOC_FAILED_HOOK    1

#define configSUPPORT_STATIC_ALLOCATION 0
#define configSUPPORT_DYNAMIC_ALLOCATION 1

#define configTIMER_TASK_PRIORITY       3
#define configTIMER_QUEUE_LENGTH        10
#define configTIMER_TASK_STACK_DEPTH    256

#define configRUN_MULTIPLE_PRIORITIES    0

#define configUSE_STATS_FORMATTING_FUNCTIONS 0

#define configGENERATE_RUN_TIME_STATS   0
#define portCONFIGURE_TIMER_FOR_RUN_TIME_STATS()
#define portGET_RUN_TIME_COUNTER_VALUE() 0

#define configNUM_THREAD_LOCAL_STORAGE_POINTERS 0

#define INCLUDE_vTaskPrioritySet        1
#define INCLUDE_uxTaskPriorityGet       1
#define INCLUDE_vTaskDelete             1
#define INCLUDE_vTaskSuspend            1
#define INCLUDE_vTaskDelayUntil         1
#define INCLUDE_vTaskDelay              1
#define INCLUDE_xTaskGetSchedulerState  1
#define INCLUDE_xTaskGetCurrentTaskHandle 1
#define INCLUDE_uxTaskGetStackHighWaterMark 1
#define INCLUDE_xTaskGetIdleTaskHandle  0
#define INCLUDE_eTaskGetState           0
#define INCLUDE_xEventGroupSetBitFromISR 1
#define INCLUDE_xTimerPendFunctionCall  1
#define INCLUDE_xTaskAbortDelay         0
#define INCLUDE_xTaskGetHandle          0
#define INCLUDE_xTaskResumeFromISR      1

#define configASSERT_DEFINED 1
#define configASSERT(x) if(!(x)) { taskDISABLE_INTERRUPTS(); __asm volatile("bkpt #0"); }

#define vPortSVCHandler                 isr_svcall
#define vPortPendSVHandler              isr_pendsv
#define vPortSysTickHandler             isr_systick

#endif