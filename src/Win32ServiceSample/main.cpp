#include <windows.h>
#include <tchar.h>
#include <strsafe.h>
#include <atlstr.h>

#pragma comment(lib, "advapi32.lib")

const wchar_t SVCNAME[] = L"SvcName";
constexpr int SVC_ERROR = 1;

SERVICE_STATUS          gSvcStatus;
SERVICE_STATUS_HANDLE   gSvcStatusHandle;
HANDLE                  ghSvcStopEvent = nullptr;

void SvcInstall(void);
DWORD WINAPI SvcCtrlHandler(DWORD dwCtrl, DWORD eventType, LPVOID lpEventData, LPVOID lpContext);
void WINAPI SvcMain(DWORD, LPTSTR*);

void ReportSvcStatus(DWORD, DWORD, DWORD);
void SvcInit(DWORD, LPTSTR*);
void SvcReportEvent(const wchar_t* szFunction);


//
// Purpose: 
//   Entry point for the process
//
// Parameters:
//   None
// 
// Return value:
//   None
//
int __cdecl _tmain(int argc, TCHAR* argv[])
{
    // If command-line parameter is "install", install the service. 
    // Otherwise, the service is probably being started by the SCM.

    if (lstrcmpi(argv[1], TEXT("install")) == 0)
    {
        SvcInstall();
        return 0;
    }

    LPWSTR svc = LPWSTR(SVCNAME);

    // TO_DO: Add any additional services for the process to this table.
    SERVICE_TABLE_ENTRY DispatchTable[] =
    {
        { svc, (LPSERVICE_MAIN_FUNCTION)SvcMain },
        { NULL, NULL }
    };

    // This call returns when the service has stopped. 
    // The process should simply terminate when the call returns.

    if (StartServiceCtrlDispatcherW(DispatchTable) == false)
    {
        SvcReportEvent(L"StartServiceCtrlDispatcher");
    }

    return 0;
}

//
// Purpose: 
//   Installs a service in the SCM database
//
// Parameters:
//   None
// 
// Return value:
//   None
//
VOID SvcInstall()
{
    SC_HANDLE schSCManager;
    SC_HANDLE schService;
    TCHAR szPath[MAX_PATH];

    if (GetModuleFileNameW(nullptr, szPath, MAX_PATH) == false)
    {
        printf("Cannot install service (%d)\n", GetLastError());
        return;
    }

    // Get a handle to the SCM database. 

    schSCManager = OpenSCManagerW(
        nullptr,                    // local computer
        nullptr,                    // ServicesActive database 
        SC_MANAGER_ALL_ACCESS       // full access rights 
    );

    if (schSCManager == nullptr)
    {
        printf("OpenSCManager failed (%d)\n", GetLastError());
        return;
    }

    // Create the service

    schService = CreateServiceW(
        schSCManager,              // SCM database 
        SVCNAME,                   // name of service 
        SVCNAME,                   // service name to display 
        SERVICE_ALL_ACCESS,        // desired access 
        SERVICE_WIN32_OWN_PROCESS, // service type 
        SERVICE_DEMAND_START,      // start type 
        SERVICE_ERROR_NORMAL,      // error control type 
        szPath,                    // path to service's binary 
        nullptr,                      // no load ordering group 
        nullptr,                      // no tag identifier 
        nullptr,                      // no dependencies 
        nullptr,                      // LocalSystem account 
        nullptr                        // no password 
    );                     

    if (schService == nullptr)
    {
        printf("CreateService failed (%d)\n", GetLastError());
        CloseServiceHandle(schSCManager);
        return;
    }
    printf("Service installed successfully\n");

    CloseServiceHandle(schService);
    CloseServiceHandle(schSCManager);
}

//
// Purpose: 
//   Entry point for the service
//
// Parameters:
//   dwArgc - Number of arguments in the lpszArgv array
//   lpszArgv - Array of strings. The first string is the name of
//     the service and subsequent strings are passed by the process
//     that called the StartService function to start the service.
// 
// Return value:
//   None.
//
void WINAPI SvcMain(DWORD dwArgc, LPTSTR* lpszArgv)
{
    // Register the handler function for the service
    gSvcStatusHandle = RegisterServiceCtrlHandlerExW(
        SVCNAME,
        SvcCtrlHandler,
        nullptr         // can pass a pointer here
    );

    if (gSvcStatusHandle == nullptr)
    {
        SvcReportEvent(TEXT("RegisterServiceCtrlHandler"));
        return;
    }

    // These SERVICE_STATUS members remain as set here
    gSvcStatus.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
    gSvcStatus.dwServiceSpecificExitCode = 0;

    // Report initial status to the SCM

    ReportSvcStatus(SERVICE_START_PENDING, NO_ERROR, 3000);

    // Perform service-specific initialization and work.

    SvcInit(dwArgc, lpszArgv);
}

//
// Purpose: 
//   The service code
//
// Parameters:
//   dwArgc - Number of arguments in the lpszArgv array
//   lpszArgv - Array of strings. The first string is the name of
//     the service and subsequent strings are passed by the process
//     that called the StartService function to start the service.
// 
// Return value:
//   None
//
VOID SvcInit(DWORD dwArgc, LPTSTR* lpszArgv)
{
    // TO_DO: Declare and set any required variables.
    //   Be sure to periodically call ReportSvcStatus() with 
    //   SERVICE_START_PENDING. If initialization fails, call
    //   ReportSvcStatus with SERVICE_STOPPED.

    // Create an event. The control handler function, SvcCtrlHandler,
    // signals this event when it receives the stop control code.

    ghSvcStopEvent = CreateEventW(
        nullptr,    // default security attributes
        true,    // manual reset event
        false,   // not signaled
        nullptr // no name
    );   

    if (ghSvcStopEvent == nullptr)
    {
        ReportSvcStatus(SERVICE_STOPPED, NO_ERROR, 0);
        return;
    }

    // Report running status when initialization is complete.

    ReportSvcStatus(SERVICE_RUNNING, NO_ERROR, 0);

    // TO_DO: Perform work until service stops.

    while (1)
    {
        // Check whether to stop the service.

        WaitForSingleObject(ghSvcStopEvent, INFINITE);

        ReportSvcStatus(SERVICE_STOPPED, NO_ERROR, 0);
        return;
    }
}

//
// Purpose: 
//   Sets the current service status and reports it to the SCM.
//
// Parameters:
//   dwCurrentState - The current state (see SERVICE_STATUS)
//   dwWin32ExitCode - The system error code
//   dwWaitHint - Estimated time for pending operation, 
//     in milliseconds
// 
// Return value:
//   None
//
void ReportSvcStatus(
    DWORD dwCurrentState,
    DWORD dwWin32ExitCode,
    DWORD dwWaitHint
)
{
    static DWORD dwCheckPoint = 1;

    // Fill in the SERVICE_STATUS structure.

    gSvcStatus.dwCurrentState = dwCurrentState;
    gSvcStatus.dwWin32ExitCode = dwWin32ExitCode;
    gSvcStatus.dwWaitHint = dwWaitHint;

    // Add other controls as required https://docs.microsoft.com/en-us/windows/win32/api/winsvc/ns-winsvc-service_status
    constexpr static DWORD acceptedControlCodes = SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN;
    switch (dwCurrentState)
    {
        case SERVICE_START_PENDING:
            gSvcStatus.dwControlsAccepted = 0;
            gSvcStatus.dwCheckPoint = dwCheckPoint++;
            break;

        case SERVICE_RUNNING:
        case SERVICE_STOPPED:
            gSvcStatus.dwControlsAccepted = acceptedControlCodes;
            gSvcStatus.dwCheckPoint = 0;
            break;

        default:
            gSvcStatus.dwControlsAccepted = acceptedControlCodes;
            gSvcStatus.dwCheckPoint = dwCheckPoint++;
    }

    // Report the status of the service to the SCM.
    SetServiceStatus(gSvcStatusHandle, &gSvcStatus);
}

//
// Purpose: 
//   Called by SCM whenever a control code is sent to the service
//   using the ControlService function.
//
// Parameters:
//   dwCtrl - control code
// 
// https://docs.microsoft.com/en-us/previous-versions/windows/it-pro/windows-vista/cc721961(v=ws.10)?redirectedfrom=MSDN#using-service-control-manager-scm-notifications
// https://docs.microsoft.com/en-us/windows/win32/api/winsvc/nc-winsvc-lphandler_function_ex
// https://docs.microsoft.com/en-us/windows/win32/api/winsvc/ns-winsvc-service_status?redirectedfrom=MSDN
// https://docs.microsoft.com/en-us/windows/win32/api/winsvc/nf-winsvc-registerservicectrlhandlerexa
DWORD WINAPI SvcCtrlHandler(DWORD dwCtrl, DWORD eventType, LPVOID lpEventData, LPVOID lpContext)
{
    // Handle the requested control code. 
    // See return value notes in https://docs.microsoft.com/en-us/windows/win32/api/winsvc/nc-winsvc-lphandler_function_ex
    switch (dwCtrl)
    {
        // If your service handles SERVICE_CONTROL_STOP or SERVICE_CONTROL_SHUTDOWN, return NO_ERROR.
        case SERVICE_CONTROL_STOP:
            ReportSvcStatus(SERVICE_STOP_PENDING, NO_ERROR, 0);

            // Signal the service to stop.
            SetEvent(ghSvcStopEvent);
            ReportSvcStatus(gSvcStatus.dwCurrentState, NO_ERROR, 0);

            return NO_ERROR;

        // If your service handles SERVICE_CONTROL_STOP or SERVICE_CONTROL_SHUTDOWN, return NO_ERROR.
        case SERVICE_CONTROL_SHUTDOWN:
            return NO_ERROR;

        // In general, if your service does not handle the control, return ERROR_CALL_NOT_IMPLEMENTED. 
        // However, your service should return NO_ERROR for SERVICE_CONTROL_INTERROGATE even if your 
        // service does not handle it.
        case SERVICE_CONTROL_INTERROGATE:
            return NO_ERROR;

        // In general, if your service does not handle the control, return ERROR_CALL_NOT_IMPLEMENTED. 
        // However, your service should return NO_ERROR for SERVICE_CONTROL_INTERROGATE even if your 
        // service does not handle it.
        default:
            return ERROR_CALL_NOT_IMPLEMENTED;

        // If your service handles SERVICE_CONTROL_DEVICEEVENT, return NO_ERROR to grant the request and an error code to deny the request.
        // If your service handles SERVICE_CONTROL_HARDWAREPROFILECHANGE, return NO_ERROR to grant the request and an error code to deny the request.
        // If your service handles SERVICE_CONTROL_POWEREVENT, return NO_ERROR to grant the request and an error code to deny the request.
        // For all other control codes your service handles, return NO_ERROR.
    }
}

//
// Purpose: 
//   Logs messages to the event log
//
// Parameters:
//   szFunction - name of function that failed
// 
// Return value:
//   None
//
// Remarks:
//   The service must have an entry in the Application event log.
//
void SvcReportEvent(const wchar_t* szFunction)
{
    HANDLE hEventSource = RegisterEventSourceW(nullptr, SVCNAME);

    if (hEventSource != nullptr)
    {
        TCHAR Buffer[80];
        StringCchPrintf(Buffer, 80, TEXT("%s failed with %d"), szFunction, GetLastError());

        LPCTSTR lpszStrings[2];
        lpszStrings[0] = SVCNAME;
        lpszStrings[1] = Buffer;

        ReportEventW(
            hEventSource,        // event log handle
            EVENTLOG_ERROR_TYPE, // event type
            0,                   // event category
            SVC_ERROR,           // event identifier
            nullptr,                // no security identifier
            2,                   // size of lpszStrings array
            0,                   // no binary data
            lpszStrings,         // array of strings
            nullptr             // no binary data
        );               

        DeregisterEventSource(hEventSource);
    }
}