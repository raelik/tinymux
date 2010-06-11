/*! \file funcs.h
 * \brief Funcs Module
 *
 * $Id$
 *
 */

#ifndef FUNCS_H
#define FUNCS_H

const MUX_CID CID_Funcs       = UINT64_C(0x000000026552A4A8);

interface IFunctionSink : public mux_IUnknown
{
public:
    virtual void Unregistering(void) = 0;
};

class CFuncs : public IFunctionSink
{
private:
    mux_ILog                 *m_pILog;
    mux_IFunctionControl     *m_pIFunctionControl;

public:
    // mux_IUnknown
    //
    virtual MUX_RESULT QueryInterface(MUX_IID iid, void **ppv);
    virtual UINT32     AddRef(void);
    virtual UINT32     Release(void);

    // IFuncs
    //
    virtual void Unregistering(void);

    CFuncs(void);
    MUX_RESULT FinalConstruct(void);
    virtual ~CFuncs();

private:
    UINT32        m_cRef;
};

class CFuncsFactory : public mux_IClassFactory
{
public:
    // mux_IUnknown
    //
    virtual MUX_RESULT QueryInterface(MUX_IID iid, void **ppv);
    virtual UINT32     AddRef(void);
    virtual UINT32     Release(void);

    // mux_IClassFactory
    //
    virtual MUX_RESULT CreateInstance(mux_IUnknown *pUnknownOuter, MUX_IID iid, void **ppv);
    virtual MUX_RESULT LockServer(bool bLock);

    CFuncsFactory(void);
    virtual ~CFuncsFactory();

private:
    UINT32 m_cRef;
};

#endif // FUNCS_H
