// Low-level USB device interface (PCD <-> USBD) for minimal CDC build
#include "stm32f4xx_hal.h"
#include "usbd_conf.h"   // included first per ST stack requirements
#include "usbd_core.h"   // pulls in usbd_def.h after usbd_conf.h
#include "usbd_cdc.h"
#include "usbd_desc.h"
#include <stdio.h>

/* Emergency override: force enabling internal FS PHY pull-up & power if normal init path fails.
 * Define FORCE_USB_PULLUP=1 at compile time to always execute, or leave at 0 and we will only
 * try this when explicitly asked via helper function. */
#ifndef FORCE_USB_PULLUP
#define FORCE_USB_PULLUP 1
#endif

/* Low level PCD/USB FS implementation adapted for minimal CDC build */

PCD_HandleTypeDef hpcd_USB_OTG_FS;

/* --- Static allocation for USBD core (very small needs for CDC) --- */
static uint32_t usbd_static_mem[(sizeof(void*) * 32 + 3) / 4]; /* ~128 bytes (adjust if needed) */

void *USBD_static_malloc(uint32_t size) {
    if (size > sizeof(usbd_static_mem)) return NULL; /* simplistic allocator */
    return usbd_static_mem;
}
void USBD_static_free(void *p) { (void)p; /* no-op */ }

void OTG_FS_IRQHandler(void)
{
    HAL_PCD_IRQHandler(&hpcd_USB_OTG_FS);
}

void HAL_PCD_MspInit(PCD_HandleTypeDef* pcdHandle) {
    if (pcdHandle->Instance == USB_OTG_FS) {
        __HAL_RCC_GPIOA_CLK_ENABLE();
        __HAL_RCC_USB_OTG_FS_CLK_ENABLE();
        GPIO_InitTypeDef GPIO_InitStruct = {0};
        GPIO_InitStruct.Pin = GPIO_PIN_11|GPIO_PIN_12; // DM, DP
        GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
        GPIO_InitStruct.Alternate = GPIO_AF10_OTG_FS;
        HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
        HAL_NVIC_SetPriority(OTG_FS_IRQn, 5, 0);
        HAL_NVIC_EnableIRQ(OTG_FS_IRQn);
    }
}

void HAL_PCD_MspDeInit(PCD_HandleTypeDef* pcdHandle) {
    if (pcdHandle->Instance == USB_OTG_FS) {
        __HAL_RCC_USB_OTG_FS_CLK_DISABLE();
        HAL_GPIO_DeInit(GPIOA, GPIO_PIN_11|GPIO_PIN_12);
        HAL_NVIC_DisableIRQ(OTG_FS_IRQn);
    }
}

USBD_StatusTypeDef USBD_LL_Init(USBD_HandleTypeDef *pdev) {
    hpcd_USB_OTG_FS.Instance = USB_OTG_FS;
    hpcd_USB_OTG_FS.Init.dev_endpoints = 6;
    hpcd_USB_OTG_FS.Init.speed = PCD_SPEED_FULL;
    hpcd_USB_OTG_FS.Init.dma_enable = DISABLE;
    hpcd_USB_OTG_FS.Init.phy_itface = PCD_PHY_EMBEDDED;
    hpcd_USB_OTG_FS.Init.Sof_enable = DISABLE;
    hpcd_USB_OTG_FS.Init.low_power_enable = DISABLE;
    hpcd_USB_OTG_FS.Init.lpm_enable = DISABLE;
    hpcd_USB_OTG_FS.Init.vbus_sensing_enable = DISABLE; /* board has no VBUS sensing pin wired */
    hpcd_USB_OTG_FS.Init.use_dedicated_ep1 = DISABLE;
    if (HAL_PCD_Init(&hpcd_USB_OTG_FS) != HAL_OK) return USBD_FAIL;
    /* Configure FIFO sizes (values in 32-bit words). Tune if needed. */
    HAL_PCDEx_SetRxFiFo(&hpcd_USB_OTG_FS, 0x80);      /* 128 words for OUT packets + setup */
    HAL_PCDEx_SetTxFiFo(&hpcd_USB_OTG_FS, 0, 0x40);   /* EP0 IN */
    HAL_PCDEx_SetTxFiFo(&hpcd_USB_OTG_FS, 1, 0x80);   /* CDC Data IN (EP1 IN) */
    HAL_PCDEx_SetTxFiFo(&hpcd_USB_OTG_FS, 2, 0x40);   /* CDC Cmd IN (EP2 IN) */
    /* Some boards (no VBUS sensing) need explicit power-down clear & optional VBUS detector bits.
     * Ensure core power down bit cleared so internal pull-up can drive D+. */
    USB_OTG_FS->GCCFG |= USB_OTG_GCCFG_PWRDWN; /* Exit power down */
#if FORCE_USB_PULLUP
    /* Optionally enable B-device VBUS sensing to satisfy ST core state machine even if pin not wired. */
    USB_OTG_FS->GCCFG |= USB_OTG_GCCFG_VBUSBSEN; /* fake VBUS B-sensing */
#endif
    __HAL_PCD_ENABLE(&hpcd_USB_OTG_FS);
    pdev->pData = &hpcd_USB_OTG_FS;
    hpcd_USB_OTG_FS.pData = pdev;
    /* Debug dump */
    printf("[USBDBG] After LL_Init: GCCFG=0x%08lX GUSBCFG=0x%08lX DCFG=0x%08lX DCTL=0x%08lX GRXFSIZ=0x%08lX\n",
           (unsigned long)USB_OTG_FS->GCCFG,
           (unsigned long)USB_OTG_FS->GUSBCFG,
           (unsigned long)USB_OTG_FS->DCFG,
           (unsigned long)USB_OTG_FS->DCTL,
           (unsigned long)USB_OTG_FS->GRXFSIZ);
    return USBD_OK;
}

USBD_StatusTypeDef USBD_LL_DeInit(USBD_HandleTypeDef *pdev) {
    (void)pdev;
    HAL_PCD_DeInit(&hpcd_USB_OTG_FS);
    return USBD_OK;
}

USBD_StatusTypeDef USBD_LL_Start(USBD_HandleTypeDef *pdev) { (void)pdev; HAL_PCD_Start(&hpcd_USB_OTG_FS); return USBD_OK; }
USBD_StatusTypeDef USBD_LL_Stop(USBD_HandleTypeDef *pdev) { (void)pdev; HAL_PCD_Stop(&hpcd_USB_OTG_FS); return USBD_OK; }

USBD_StatusTypeDef USBD_LL_OpenEP(USBD_HandleTypeDef *pdev, uint8_t ep_addr, uint8_t ep_type, uint16_t ep_mps) {
    (void)pdev; if (HAL_PCD_EP_Open(&hpcd_USB_OTG_FS, ep_addr, ep_mps, ep_type) != HAL_OK) return USBD_FAIL; return USBD_OK; }
USBD_StatusTypeDef USBD_LL_CloseEP(USBD_HandleTypeDef *pdev, uint8_t ep_addr) { (void)pdev; if (HAL_PCD_EP_Close(&hpcd_USB_OTG_FS, ep_addr) != HAL_OK) return USBD_FAIL; return USBD_OK; }
USBD_StatusTypeDef USBD_LL_FlushEP(USBD_HandleTypeDef *pdev, uint8_t ep_addr) { (void)pdev; if (HAL_PCD_EP_Flush(&hpcd_USB_OTG_FS, ep_addr) != HAL_OK) return USBD_FAIL; return USBD_OK; }
USBD_StatusTypeDef USBD_LL_StallEP(USBD_HandleTypeDef *pdev, uint8_t ep_addr) { (void)pdev; if (HAL_PCD_EP_SetStall(&hpcd_USB_OTG_FS, ep_addr) != HAL_OK) return USBD_FAIL; return USBD_OK; }
USBD_StatusTypeDef USBD_LL_ClearStallEP(USBD_HandleTypeDef *pdev, uint8_t ep_addr) { (void)pdev; if (HAL_PCD_EP_ClrStall(&hpcd_USB_OTG_FS, ep_addr) != HAL_OK) return USBD_FAIL; return USBD_OK; }
uint8_t USBD_LL_IsStallEP(USBD_HandleTypeDef *pdev, uint8_t ep_addr) { (void)pdev; if((ep_addr & 0x80U) == 0x80U) return hpcd_USB_OTG_FS.IN_ep[ep_addr & 0x7FU].is_stall; return hpcd_USB_OTG_FS.OUT_ep[ep_addr & 0x7FU].is_stall; }
USBD_StatusTypeDef USBD_LL_SetUSBAddress(USBD_HandleTypeDef *pdev, uint8_t dev_addr) { (void)pdev; HAL_PCD_SetAddress(&hpcd_USB_OTG_FS, dev_addr); return USBD_OK; }
// In recent Cube releases USBD_EndpointTypeDef no longer exposes xfer_* to the user layer.
// The core passes buffers directly to HAL_PCD_EP_Transmit/Receive so we simply accept them.
USBD_StatusTypeDef USBD_LL_SetTxBuffer(USBD_HandleTypeDef *pdev, uint8_t ep_addr, uint8_t *pbuf, uint32_t size) {
    (void)pdev; (void)ep_addr; (void)pbuf; (void)size; return USBD_OK; }
USBD_StatusTypeDef USBD_LL_SetRxBuffer(USBD_HandleTypeDef *pdev, uint8_t ep_addr, uint8_t *pbuf) {
    (void)pdev; (void)ep_addr; (void)pbuf; return USBD_OK; }

USBD_StatusTypeDef USBD_LL_Transmit(USBD_HandleTypeDef *pdev, uint8_t ep_addr, uint8_t *pbuf, uint32_t size) {
    (void)pdev;
    if (HAL_PCD_EP_Transmit(&hpcd_USB_OTG_FS, ep_addr, pbuf, size) != HAL_OK) {
        return USBD_FAIL;
    }
    return USBD_OK;
}
USBD_StatusTypeDef USBD_LL_PrepareReceive(USBD_HandleTypeDef *pdev, uint8_t ep_addr, uint8_t *pbuf, uint32_t size) {
    (void)pdev;
    if (HAL_PCD_EP_Receive(&hpcd_USB_OTG_FS, ep_addr, pbuf, size) != HAL_OK) {
        return USBD_FAIL;
    }
    return USBD_OK;
}

uint32_t USBD_LL_GetRxDataSize(USBD_HandleTypeDef *pdev, uint8_t ep_addr) {
    PCD_HandleTypeDef *hpcd = (PCD_HandleTypeDef*)pdev->pData;
    return HAL_PCD_EP_GetRxCount(hpcd, ep_addr & 0x7FU);
}

void HAL_PCD_SOFCallback(PCD_HandleTypeDef *hpcd) { USBD_LL_SOF((USBD_HandleTypeDef*)hpcd->pData); }
void HAL_PCD_SetupStageCallback(PCD_HandleTypeDef *hpcd) { USBD_LL_SetupStage((USBD_HandleTypeDef*)hpcd->pData, (uint8_t*)hpcd->Setup); }
void HAL_PCD_DataOutStageCallback(PCD_HandleTypeDef *hpcd, uint8_t epnum) { USBD_LL_DataOutStage((USBD_HandleTypeDef*)hpcd->pData, epnum, hpcd->OUT_ep[epnum].xfer_buff); }
void HAL_PCD_DataInStageCallback(PCD_HandleTypeDef *hpcd, uint8_t epnum) { USBD_LL_DataInStage((USBD_HandleTypeDef*)hpcd->pData, epnum, hpcd->IN_ep[epnum].xfer_buff); }
void HAL_PCD_ResetCallback(PCD_HandleTypeDef *hpcd) { USBD_LL_Reset((USBD_HandleTypeDef*)hpcd->pData); USBD_LL_SetSpeed((USBD_HandleTypeDef*)hpcd->pData, USBD_SPEED_FULL); }
void HAL_PCD_SuspendCallback(PCD_HandleTypeDef *hpcd) { USBD_LL_Suspend((USBD_HandleTypeDef*)hpcd->pData); }
void HAL_PCD_ResumeCallback(PCD_HandleTypeDef *hpcd) { USBD_LL_Resume((USBD_HandleTypeDef*)hpcd->pData); }
void HAL_PCD_ISOOUTIncompleteCallback(PCD_HandleTypeDef *hpcd, uint8_t epnum) { USBD_LL_IsoOUTIncomplete((USBD_HandleTypeDef*)hpcd->pData, epnum); }
void HAL_PCD_ISOINIncompleteCallback(PCD_HandleTypeDef *hpcd, uint8_t epnum) { USBD_LL_IsoINIncomplete((USBD_HandleTypeDef*)hpcd->pData, epnum); }
void HAL_PCD_ConnectCallback(PCD_HandleTypeDef *hpcd) { USBD_LL_DevConnected((USBD_HandleTypeDef*)hpcd->pData); }
void HAL_PCD_DisconnectCallback(PCD_HandleTypeDef *hpcd) { USBD_LL_DevDisconnected((USBD_HandleTypeDef*)hpcd->pData); }

void USBD_LL_Delay(uint32_t Delay) { HAL_Delay(Delay); }

 
 S u n d a y ,   S e p t e m b e r   2 8 ,   2 0 2 5   8 : 0 0 : 0 0   P M 
 
 
 
 
 
 