/*! \file funcs.cpp
 * \brief Funcs Module
 *
 * $Id$
 *
 */

#include "../autoconf.h"
#include "../config.h"
#include "../libmux.h"
#include "../modules.h"
#include "funcs.h"

static INT32 g_cComponents  = 0;
static INT32 g_cServerLocks = 0;

static IFunctionSink *g_pIFunctionSink = NULL;

static CLASS_INFO funcs_classes[] =
{
    { CID_Funcs }
};
#define NUM_CLASSES (sizeof(funcs_classes)/sizeof(funcs_classes[0]))

// The following four functions are for access by dlopen.
//
extern "C" MUX_RESULT DCL_EXPORT DCL_API mux_CanUnloadNow(void)
{
    if (  0 == g_cComponents
       && 0 == g_cServerLocks)
    {
        return MUX_S_OK;
    }
    else
    {
        return MUX_S_FALSE;
    }
}

extern "C" MUX_RESULT DCL_EXPORT DCL_API mux_GetClassObject(MUX_CID cid, MUX_IID iid, void **ppv)
{
    MUX_RESULT mr = MUX_E_CLASSNOTAVAILABLE;

    if (CID_Funcs == cid)
    {
        CFuncsFactory *pFuncsFactory = NULL;
        try
        {
            pFuncsFactory = new CFuncsFactory;
        }
        catch (...)
        {
            ; // Nothing.
        }

        if (NULL == pFuncsFactory)
        {
            return MUX_E_OUTOFMEMORY;
        }

        mr = pFuncsFactory->QueryInterface(iid, ppv);
        pFuncsFactory->Release();
    }
    return mr;
}

extern "C" MUX_RESULT DCL_EXPORT DCL_API mux_Register(void)
{
    MUX_RESULT mr = MUX_E_UNEXPECTED;

    if (NULL == g_pIFunctionSink)
    {
        // Advertise our components.
        //
        mr = mux_RegisterClassObjects(NUM_CLASSES, funcs_classes, NULL);
        if (MUX_FAILED(mr))
        {
            return mr;
        }

        // Create an instance of our CFuncs component.
        //
        IFunctionSink *pIFunctionSink = NULL;
        mr = mux_CreateInstance(CID_Funcs, NULL, UseSameProcess, IID_IFunctionSink, (void **)&pIFunctionSink);
        if (MUX_SUCCEEDED(mr))
        {
            g_pIFunctionSink = pIFunctionSink;
            pIFunctionSink = NULL;
        }
        else
        {
            (void)mux_RevokeClassObjects(NUM_CLASSES, funcs_classes);
            mr = MUX_E_OUTOFMEMORY;
        }
    }
    return mr;
}

extern "C" MUX_RESULT DCL_EXPORT DCL_API mux_Unregister(void)
{
    // Destroy our CFuncs component.
    //
    if (NULL != g_pIFunctionSink)
    {
        g_pIFunctionSink->Unregistering();
        g_pIFunctionSink->Release();
        g_pIFunctionSink = NULL;
    }

    return mux_RevokeClassObjects(NUM_CLASSES, funcs_classes);
}

// Funcs component which is not directly accessible.
//
CFuncs::CFuncs(void) : m_cRef(1)
{
    g_cComponents++;
    m_pILog = NULL;
    m_pIFunctionControl = NULL;
}

MUX_RESULT CFuncs::FinalConstruct(void)
{
    MUX_RESULT mr;

    // Use CLog provided by netmux.
    //
    mr = mux_CreateInstance(CID_Log, NULL, UseSameProcess, IID_ILog, (void **)&m_pILog);
    if (MUX_SUCCEEDED(mr))
    {
        if (m_pILog->start_log(LOG_ALWAYS, T("INI"), T("INFO")))
        {
            m_pILog->log_printf(T("CFuncs::CFuncs()."));
            m_pILog->end_log();
        }
    }

    // Start conversation with netmux to offer softcode functions.
    //
    mux_IFunctionSink *pIFunctionSink = NULL;
    mr = QueryInterface(IID_IFunctionSink, (void **)&pIFunctionSink);
    if (MUX_SUCCEEDED(mr))
    {
        mr = mux_CreateInstance(CID_FunctionSource, NULL, UseSameProcess, IID_IFunctionControl, (void **)&m_pIFunctionControl);
        if (MUX_SUCCEEDED(mr))
        {
            m_pIFunctionControl->Advise(pIFunctionSink);
        }
        pIFunctionSink->Release();
    }

    return mr;
}

CFuncs::~CFuncs()
{
    if (NULL != m_pILog)
    {
        if (m_pILog->start_log(LOG_ALWAYS, T("INI"), T("INFO")))
        {
            m_pILog->log_printf(T("CFuncs::~CFuncs()"));
            m_pILog->end_log();
        }

        m_pILog->Release();
        m_pILog = NULL;
    }

    if (NULL != m_pIFunctionControl)
    {
        m_pIFunctionControl->Release();
        m_pIFunctionControl = NULL;
    }

    g_cComponents--;
}

MUX_RESULT CFuncs::QueryInterface(MUX_IID iid, void **ppv)
{
    if (mux_IID_IUnknown == iid)
    {
        *ppv = static_cast<IFunctionSink *>(this);
    }
    else if (IID_IFunctionSink == iid)
    {
        *ppv = static_cast<IFunctionSink *>(this);
    }
    else
    {
        *ppv = NULL;
        return MUX_E_NOINTERFACE;
    }
    reinterpret_cast<mux_IUnknown *>(*ppv)->AddRef();
    return MUX_S_OK;
}

UINT32 CFuncs::AddRef(void)
{
    m_cRef++;
    return m_cRef;
}

UINT32 CFuncs::Release(void)
{
    m_cRef--;
    if (0 == m_cRef)
    {
        delete this;
        return 0;
    }
    return m_cRef;
}

void CFuncs::Unregistering(void)
{
    // We need to release our references before netmux will release his.
    //
    if (NULL != m_pIFunctionControl)
    {
        m_pIFunctionControl->Release();
        m_pIFunctionControl = NULL;
    }
}

// Factory for Funcs component which is not directly accessible.
//
CFuncsFactory::CFuncsFactory(void) : m_cRef(1)
{
}

CFuncsFactory::~CFuncsFactory()
{
}

MUX_RESULT CFuncsFactory::QueryInterface(MUX_IID iid, void **ppv)
{
    if (mux_IID_IUnknown == iid)
    {
        *ppv = static_cast<mux_IClassFactory *>(this);
    }
    else if (mux_IID_IClassFactory == iid)
    {
        *ppv = static_cast<mux_IClassFactory *>(this);
    }
    else
    {
        *ppv = NULL;
        return MUX_E_NOINTERFACE;
    }
    reinterpret_cast<mux_IUnknown *>(*ppv)->AddRef();
    return MUX_S_OK;
}

UINT32 CFuncsFactory::AddRef(void)
{
    m_cRef++;
    return m_cRef;
}

UINT32 CFuncsFactory::Release(void)
{
    m_cRef--;
    if (0 == m_cRef)
    {
        delete this;
        return 0;
    }
    return m_cRef;
}

MUX_RESULT CFuncsFactory::CreateInstance(mux_IUnknown *pUnknownOuter, MUX_IID iid, void **ppv)
{
    // Disallow attempts to aggregate this component.
    //
    if (NULL != pUnknownOuter)
    {
        return MUX_E_NOAGGREGATION;
    }

    CFuncs *pFuncs = NULL;
    try
    {
        pFuncs = new CFuncs;
    }
    catch (...)
    {
        ; // Nothing.
    }

    MUX_RESULT mr;
    if (NULL == pFuncs)
    {
        return MUX_E_OUTOFMEMORY;
    }
    else
    {
        mr = pFuncs->FinalConstruct();
        if (MUX_FAILED(mr))
        {
            pFuncs->Release();
            return mr;
        }
    }

    mr = pFuncs->QueryInterface(iid, ppv);
    pFuncs->Release();
    return mr;
}

MUX_RESULT CFuncsFactory::LockServer(bool bLock)
{
    if (bLock)
    {
        g_cServerLocks++;
    }
    else
    {
        g_cServerLocks--;
    }
    return MUX_S_OK;
}
