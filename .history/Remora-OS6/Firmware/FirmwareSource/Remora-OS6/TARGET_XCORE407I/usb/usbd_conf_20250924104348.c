#include "stm32f4xx_hal.h"
#include "usbd_def.h"
#include "usbd_core.h"

PCD_HandleTypeDef hpcd_USB_OTG_FS;

void HAL_PCD_MspInit(PCD_HandleTypeDef* pcdHandle)
{
    if(pcdHandle->Instance==USB_OTG_FS)
    {
        /* Some HAL internals may touch SYSCFG for OTG features */
        __HAL_RCC_SYSCFG_CLK_ENABLE();
        __HAL_RCC_GPIOA_CLK_ENABLE();
        GPIO_InitTypeDef GPIO_InitStruct = {0};
        /* USB FS DP/DM: PA12/PA11 */
        GPIO_InitStruct.Pin = GPIO_PIN_11|GPIO_PIN_12;
        GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
        GPIO_InitStruct.Alternate = GPIO_AF10_OTG_FS;
        HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

        /* Optional: Configure VBUS (PA9) and ID (PA10) as inputs to ensure clean states
         * when vbus_sensing is disabled. Harmless on boards that don't route these. */
        GPIO_InitStruct.Pin = GPIO_PIN_9 | GPIO_PIN_10;
        GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
        HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

        __HAL_RCC_USB_OTG_FS_CLK_ENABLE();
        HAL_NVIC_SetPriority(OTG_FS_IRQn, 6, 0);
        HAL_NVIC_EnableIRQ(OTG_FS_IRQn);
    }
}

void HAL_PCD_MspDeInit(PCD_HandleTypeDef* pcdHandle)
{
    if(pcdHandle->Instance==USB_OTG_FS)
    {
        __HAL_RCC_USB_OTG_FS_CLK_DISABLE();
        HAL_GPIO_DeInit(GPIOA, GPIO_PIN_11|GPIO_PIN_12);
        HAL_NVIC_DisableIRQ(OTG_FS_IRQn);
    }
}

USBD_StatusTypeDef USBD_LL_Init(USBD_HandleTypeDef *pdev)
{
    hpcd_USB_OTG_FS.Instance = USB_OTG_FS;
    hpcd_USB_OTG_FS.Init.dev_endpoints = 4;
    hpcd_USB_OTG_FS.Init.speed = PCD_SPEED_FULL;
    hpcd_USB_OTG_FS.Init.dma_enable = DISABLE;
    hpcd_USB_OTG_FS.Init.phy_itface = PCD_PHY_EMBEDDED;
    hpcd_USB_OTG_FS.Init.Sof_enable = ENABLE;
    hpcd_USB_OTG_FS.Init.low_power_enable = DISABLE;
    hpcd_USB_OTG_FS.Init.lpm_enable = DISABLE;
    /* VBUS on connector passes through a power switch (no backfeed to PA9) -> keep sensing disabled */
    hpcd_USB_OTG_FS.Init.vbus_sensing_enable = DISABLE;
    hpcd_USB_OTG_FS.Init.use_dedicated_ep1 = DISABLE;
    if (HAL_PCD_Init(&hpcd_USB_OTG_FS) != HAL_OK)
        return USBD_FAIL;
    /* Small guard delay after core init */
    HAL_Delay(2);
    HAL_PCDEx_SetRxFiFo(&hpcd_USB_OTG_FS, 0x80);
    HAL_PCDEx_SetTxFiFo(&hpcd_USB_OTG_FS, 0, 0x40);
    HAL_PCDEx_SetTxFiFo(&hpcd_USB_OTG_FS, 1, 0x80);
    pdev->pData = &hpcd_USB_OTG_FS;
    hpcd_USB_OTG_FS.pData = pdev;
    /* Force a disconnect so the host will see a fresh attach when we later Start */
    HAL_PCD_DevDisconnect(&hpcd_USB_OTG_FS);
    HAL_Delay(5);
    return USBD_OK;
}

USBD_StatusTypeDef USBD_LL_DeInit(USBD_HandleTypeDef *pdev)
{
    (void)pdev;
    HAL_PCD_DeInit(&hpcd_USB_OTG_FS);
    return USBD_OK;
}

USBD_StatusTypeDef USBD_LL_Start(USBD_HandleTypeDef *pdev)
{
    (void)pdev;
    HAL_PCD_Start(&hpcd_USB_OTG_FS);
    /* Ensure device connect (assert DP pull-up) with a short delay */
    HAL_Delay(5);
    HAL_PCD_DevConnect(&hpcd_USB_OTG_FS);
    return USBD_OK;
}

USBD_StatusTypeDef USBD_LL_Stop(USBD_HandleTypeDef *pdev)
{
    (void)pdev;
    HAL_PCD_Stop(&hpcd_USB_OTG_FS);
    return USBD_OK;
}

USBD_StatusTypeDef USBD_LL_OpenEP(USBD_HandleTypeDef *pdev, uint8_t ep_addr, uint8_t ep_type, uint16_t ep_mps)
{ return (HAL_PCD_EP_Open(pdev->pData, ep_addr, ep_mps, ep_type)==HAL_OK)?USBD_OK:USBD_FAIL; }
USBD_StatusTypeDef USBD_LL_CloseEP(USBD_HandleTypeDef *pdev, uint8_t ep_addr)
{ return (HAL_PCD_EP_Close(pdev->pData, ep_addr)==HAL_OK)?USBD_OK:USBD_FAIL; }
USBD_StatusTypeDef USBD_LL_FlushEP(USBD_HandleTypeDef *pdev, uint8_t ep_addr)
{ return (HAL_PCD_EP_Flush(pdev->pData, ep_addr)==HAL_OK)?USBD_OK:USBD_FAIL; }
USBD_StatusTypeDef USBD_LL_StallEP(USBD_HandleTypeDef *pdev, uint8_t ep_addr)
{ return (HAL_PCD_EP_SetStall(pdev->pData, ep_addr)==HAL_OK)?USBD_OK:USBD_FAIL; }
USBD_StatusTypeDef USBD_LL_ClearStallEP(USBD_HandleTypeDef *pdev, uint8_t ep_addr)
{ return (HAL_PCD_EP_ClrStall(pdev->pData, ep_addr)==HAL_OK)?USBD_OK:USBD_FAIL; }
uint8_t USBD_LL_IsStallEP(USBD_HandleTypeDef *pdev, uint8_t ep_addr)
{
    PCD_HandleTypeDef *hpcd = (PCD_HandleTypeDef*)pdev->pData;
    if ((ep_addr & 0x80U) == 0x80U) { return hpcd->IN_ep[ep_addr & 0x7FU].is_stall; }
    else { return hpcd->OUT_ep[ep_addr & 0x7FU].is_stall; }
}
USBD_StatusTypeDef USBD_LL_SetUSBAddress(USBD_HandleTypeDef *pdev, uint8_t dev_addr)
{ HAL_PCD_SetAddress(pdev->pData, dev_addr); return USBD_OK; }
USBD_StatusTypeDef USBD_LL_Transmit(USBD_HandleTypeDef *pdev, uint8_t ep_addr, uint8_t *pbuf, uint32_t size)
{ return (HAL_PCD_EP_Transmit(pdev->pData, ep_addr, pbuf, size)==HAL_OK)?USBD_OK:USBD_FAIL; }
USBD_StatusTypeDef USBD_LL_PrepareReceive(USBD_HandleTypeDef *pdev, uint8_t ep_addr, uint8_t *pbuf, uint32_t size)
{ return (HAL_PCD_EP_Receive(pdev->pData, ep_addr, pbuf, size)==HAL_OK)?USBD_OK:USBD_FAIL; }
uint32_t USBD_LL_GetRxDataSize(USBD_HandleTypeDef *pdev, uint8_t ep_addr)
{ return HAL_PCD_EP_GetRxCount(pdev->pData, ep_addr); }

void HAL_PCD_SOFCallback(PCD_HandleTypeDef *hpcd){ USBD_SOF(hpcd->pData); }
void HAL_PCD_SetupStageCallback(PCD_HandleTypeDef *hpcd){ USBD_LL_SetupStage(hpcd->pData, (uint8_t *)hpcd->Setup); }
void HAL_PCD_DataOutStageCallback(PCD_HandleTypeDef *hpcd, uint8_t epnum){ USBD_LL_DataOutStage(hpcd->pData, epnum, hpcd->OUT_ep[epnum].xfer_buff); }
void HAL_PCD_DataInStageCallback(PCD_HandleTypeDef *hpcd, uint8_t epnum){ USBD_LL_DataInStage(hpcd->pData, epnum, hpcd->IN_ep[epnum].xfer_buff); }
void HAL_PCD_ResetCallback(PCD_HandleTypeDef *hpcd){ USBD_LL_Reset(hpcd->pData); USBD_LL_SetSpeed(hpcd->pData, USBD_SPEED_FULL); }
void HAL_PCD_SuspendCallback(PCD_HandleTypeDef *hpcd){ USBD_LL_Suspend(hpcd->pData); }
void HAL_PCD_ResumeCallback(PCD_HandleTypeDef *hpcd){ USBD_LL_Resume(hpcd->pData); }
void HAL_PCD_ISOOUTIncompleteCallback(PCD_HandleTypeDef *hpcd, uint8_t epnum){ USBD_LL_IsoOUTIncomplete(hpcd->pData, epnum); }
void HAL_PCD_ISOINIncompleteCallback(PCD_HandleTypeDef *hpcd, uint8_t epnum){ USBD_LL_IsoINIncomplete(hpcd->pData, epnum); }
void HAL_PCD_ConnectCallback(PCD_HandleTypeDef *hpcd){ USBD_LL_DevConnected(hpcd->pData); }
void HAL_PCD_DisconnectCallback(PCD_HandleTypeDef *hpcd){ USBD_LL_DevDisconnected(hpcd->pData); }

void OTG_FS_IRQHandler(void)
{
    HAL_PCD_IRQHandler(&hpcd_USB_OTG_FS);
}

void USBD_LL_Delay(uint32_t ms)
{
    HAL_Delay(ms);
}
