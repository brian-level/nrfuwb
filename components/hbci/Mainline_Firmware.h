/* Copyright 2021-2023 NXP
 *
 * NXP Confidential. This software is owned or controlled by NXP and may only
 * be used strictly in accordance with the applicable license terms.  By
 * expressly accepting such terms or by downloading, installing, activating
 * and/or otherwise using the software, you are agreeing that you have read,
 * and that you agree to comply with and are bound by, such license terms.  If
 * you do not agree to be bound by the applicable license terms, then you may
 * not retain, install, activate or otherwise use the software.
 */

#ifndef MAINLINE_FIRMWARE_H
#define MAINLINE_FIRMWARE_H

#include <stdint.h>

#ifdef UWBIOT_USE_FTR_FILE
#include "uwb_iot_ftr.h"
#else
#include "uwb_iot_ftr_default.h"
#endif

#if UWBIOT_UWBD_SR150
/*Select one of the firmware depending upon the host */
/* The following marker is used by firmware integration script, do not edit or delete these
   comments unintentionally, applicable for all the markers in this file */
/* Selection For SR150 FW Starts Here */
#   if UWBIOT_SR1XX_FW_ROW_PROD
#      define H1_IOT_SR150_MAINLINE_PROD_FW_46_41_06_0052bbfed983a1f1_bin                   heliosEncryptedMainlineFwImage
#      define H1_IOT_SR150_MAINLINE_PROD_FW_46_41_06_0052bbfed983a1f1_bin_len               heliosEncryptedMainlineFwImageLen
#   elif UWBIOT_SR1XX_FW_RHODES3_PROD
#      define SR140_SR150_FW_BX_RHODES3_PROD_20220316_a39c6fcb_B1v32_00_FB_bin              heliosEncryptedMainlineFwImage
#      define SR140_SR150_FW_BX_RHODES3_PROD_20220316_a39c6fcb_B1v32_00_FB_bin_len          heliosEncryptedMainlineFwImageLen
#   elif UWBIOT_SR1XX_FW_ROW_DEV
#      define H1_IOT_SR150_MAINLINE_DEV_FW_46_41_06_0052bbfed983a1f1_bin                    heliosEncryptedMainlineFwImage
#      define H1_IOT_SR150_MAINLINE_DEV_FW_46_41_06_0052bbfed983a1f1_bin_len                heliosEncryptedMainlineFwImageLen
#   elif UWBIOT_SR1XX_FW_RHODES3_DEV
#      define SR140_SR150_FW_BX_RHODES3_20220316_a39c6fcb_B1v32_00_FB_bin                   heliosEncryptedMainlineFwImage
#      define SR140_SR150_FW_BX_RHODES3_20220316_a39c6fcb_B1v32_00_FB_bin_len               heliosEncryptedMainlineFwImageLen
#   else
#      error "Select anyone of the FW"
#   endif
#endif //UWBIOT_UWBD_SR150

#if UWBIOT_UWBD_SR100T || UWBIOT_UWBD_SR100S
/*Select one of the firmware depending upon the host */
/* Selection For SR100T FW Starts Here */
/* Selection For SR100S FW Starts Here */
#   if UWBIOT_SR1XX_FW_RHODES3_PROD
#       define H1_MOBILE_ROW_MAINLINE_RHODES3_PROD_FW_44_00_01_572492a66aac2909_bin           heliosEncryptedMainlineFwImage
#       define H1_MOBILE_ROW_MAINLINE_RHODES3_PROD_FW_44_00_01_572492a66aac2909_bin_len       heliosEncryptedMainlineFwImageLen
#   elif UWBIOT_SR1XX_FW_RHODES3_DEV
#       define H1_MOBILE_ROW_MAINLINE_RHODES3_DEV_FW_44_00_01_572492a66aac2909_bin                heliosEncryptedMainlineFwImage
#       define H1_MOBILE_ROW_MAINLINE_RHODES3_DEV_FW_44_00_01_572492a66aac2909_bin_len            heliosEncryptedMainlineFwImageLen
#   elif UWBIOT_SR1XX_FW_ROW_PROD
#       define H1_MOBILE_ROW_MAINLINE_PROD_FW_EE_20_A0_a8b28afc11bdaf6c_bin                         heliosEncryptedMainlineFwImage
#       define H1_MOBILE_ROW_MAINLINE_PROD_FW_EE_20_A0_a8b28afc11bdaf6c_bin_len                     heliosEncryptedMainlineFwImageLen
#   elif UWBIOT_SR1XX_FW_ROW_DEV
#       define H1_MOBILE_ROW_MAINLINE_DEV_FW_EE_20_A0_a8b28afc11bdaf6c_bin                          heliosEncryptedMainlineFwImage
#       define H1_MOBILE_ROW_MAINLINE_DEV_FW_EE_20_A0_a8b28afc11bdaf6c_bin_len                      heliosEncryptedMainlineFwImageLen
#   else
#      error "Select anyone of the FW"
#   endif
#endif //UWBIOT_UWBD_SR100T || UWBIOT_UWBD_SR100S

#if UWBIOT_UWBD_SR110T
/*Select one of the firmware depending upon the host */
/* Selection For SR110T FW Starts Here */
#   if UWBIOT_SR1XX_FW_RHODES3_PROD
#       define H1_MOBILE_ROW_MAINLINE_RHODES3_PROD_FW_44_00_01_572492a66aac2909_bin           heliosEncryptedMainlineFwImage
#       define H1_MOBILE_ROW_MAINLINE_RHODES3_PROD_FW_44_00_01_572492a66aac2909_bin_len       heliosEncryptedMainlineFwImageLen
#   elif UWBIOT_SR1XX_FW_RHODES3_DEV
#       define H1_MOBILE_ROW_MAINLINE_RHODES3_DEV_FW_44_00_01_572492a66aac2909_bin                heliosEncryptedMainlineFwImage
#       define H1_MOBILE_ROW_MAINLINE_RHODES3_DEV_FW_44_00_01_572492a66aac2909_bin_len            heliosEncryptedMainlineFwImageLen
#   elif UWBIOT_SR1XX_FW_ROW_PROD
#       define H1_MOBILE_ROW_MAINLINE_PROD_FW_EE_20_A0_a8b28afc11bdaf6c_bin                         heliosEncryptedMainlineFwImage
#       define H1_MOBILE_ROW_MAINLINE_PROD_FW_EE_20_A0_a8b28afc11bdaf6c_bin_len                     heliosEncryptedMainlineFwImageLen
#   elif UWBIOT_SR1XX_FW_ROW_DEV
#       define H1_MOBILE_ROW_MAINLINE_DEV_FW_EE_20_A0_a8b28afc11bdaf6c_bin                          heliosEncryptedMainlineFwImage
#       define H1_MOBILE_ROW_MAINLINE_DEV_FW_EE_20_A0_a8b28afc11bdaf6c_bin_len                      heliosEncryptedMainlineFwImageLen
#   else
#      error "Select anyone of the FW"
#   endif
#endif //UWBIOT_UWBD_SR110T

#if UWBIOT_UWBD_SR160
/*Select one of the firmware depending upon the host */
/* The following marker is used by firmware integration script, do not edit or delete these
   comments unintentionally, applicable for all the markers in this file */
/* Selection For SR160 FW Starts Here */
#   if UWBIOT_SR1XX_FW_RHODES3_PROD || UWBIOT_SR1XX_FW_RHODES3_DEV
#       error "FW is Not Supported for RV3"
#   elif UWBIOT_SR1XX_FW_ROW_PROD
#       define H1_IOT_SR150_MAINLINE_PROD_FW_EE_40_02_746b01d41a254ace_bin                         heliosEncryptedMainlineFwImage
#       define H1_IOT_SR150_MAINLINE_PROD_FW_EE_40_02_746b01d41a254ace_bin_len                     heliosEncryptedMainlineFwImageLen
#   elif UWBIOT_SR1XX_FW_ROW_DEV
#       define H1_IOT_SR150_MAINLINE_DEV_FW_EE_40_02_746b01d41a254ace_bin                          heliosEncryptedMainlineFwImage
#       define H1_IOT_SR150_MAINLINE_DEV_FW_EE_40_02_746b01d41a254ace_bin_len                      heliosEncryptedMainlineFwImageLen
#   else
#       error "Select anyone of the FW"
#   endif
#endif //UWBIOT_UWBD_SR160T


/* FW selction End */

/* Depending on the selection, header file would be chosen
 *
 * Steps for the header file generation:
 *
 * 1) Use xxd -i ${file_name}.bin > ${file_name}.h (or similar utility)
 * 2) in the generated header file, replace ``unsigned char`` to ``const uint8_t``
 */
#ifdef IOT_Mainline_Prod_EE_40_B0_bin
#   include <IOT_Mainline_Prod_EE_40_B0.h>
#endif

#ifdef IOT_Mainline_Dev_EE_40_B0_bin
#   include <IOT_Mainline_Dev_EE_40_B0.h>
#endif

#ifdef IOT_Mainline_Prod_EE_4F_01_bin
#   include <IOT_Mainline_Prod_EE_4F_01.h>
#endif

#ifdef IOT_Mainline_Dev_EE_4F_01_bin
#   include <IOT_Mainline_Dev_EE_4F_01.h>
#endif

#ifdef H1_IOT_SR150_MAINLINE_PROD_FW_AD_42_03_253da558bab76451_bin
#   include <H1_IOT.SR150_MAINLINE_PROD_FW_AD.42.03_253da558bab76451.h>
#endif

#ifdef H1_IOT_SR150_MAINLINE_DEV_FW_AD_42_03_253da558bab76451_bin
#   include <H1_IOT.SR150_MAINLINE_DEV_FW_AD.42.03_253da558bab76451.h>
#endif

#ifdef H1_IOT_SR150_MAINLINE_PROD_FW_46_41_06_0052bbfed983a1f1_bin
#   include <H1_IOT.SR150_MAINLINE_PROD_FW_46.41.06_0052bbfed983a1f1.h>
#endif

#ifdef H1_IOT_SR150_MAINLINE_DEV_FW_46_41_06_0052bbfed983a1f1_bin
#   include <H1_IOT.SR150_MAINLINE_DEV_FW_46.41.06_0052bbfed983a1f1.h>
#endif

#ifdef IOT_Mainline_Prod_EE_4F_02_bin
#   include <IOT_Mainline_Prod_EE_4F_02.h>
#endif

#ifdef IOT_Mainline_Dev_EE_4F_02_bin
#   include <IOT_Mainline_Dev_EE_4F_02.h>
#endif

#ifdef H1_IOT_SR150_MAINLINE_DEV_FW_AD_12_02_f86c871e8eb9e432_bin
#   include <H1_IOT.SR150_MAINLINE_DEV_FW_AD.12.02_f86c871e8eb9e432.h>
#endif

#ifdef H1_IOT_SR150_MAINLINE_PROD_FW_AD_22_02_f86c871e8eb9e432_bin
#   include <H1_IOT.SR150_MAINLINE_PROD_FW_AD.22.02_f86c871e8eb9e432.h>
#endif

#ifdef H1_IOT_SR150_MAINLINE_PROD_FW_46_41_03_d35a37071f42eed4_bin
#   include <H1_IOT.SR150_MAINLINE_PROD_FW_46.41.03_d35a37071f42eed4.h>
#endif

#ifdef H1_IOT_SR150_MAINLINE_DEV_FW_46_41_03_d35a37071f42eed4_bin
#   include <H1_IOT.SR150_MAINLINE_DEV_FW_46.41.03_d35a37071f42eed4.h>
#endif

#ifdef H1_IOT_SR150_MAINLINE_PROD_FW_46_41_01_fe58b4e0def9bc65_bin
#   include <H1_IOT.SR150_MAINLINE_PROD_FW_46.41.01_fe58b4e0def9bc65.h>
#endif

#ifdef H1_IOT_SR150_MAINLINE_DEV_FW_46_41_01_fe58b4e0def9bc65_bin
#   include <H1_IOT.SR150_MAINLINE_DEV_FW_46.41.01_fe58b4e0def9bc65.h>
#endif

#ifdef H1_MOBILE_ROW_MAINLINE_RHODES3_PROD_FW_44_00_01_572492a66aac2909_bin
#   include <H1_MOBILE_ROW_MAINLINE_RHODES3_PROD_FW_44.00.01_572492a66aac2909.h>
#endif

#ifdef H1_MOBILE_ROW_MAINLINE_RHODES3_DEV_FW_44_00_01_572492a66aac2909_bin
#   include <H1_MOBILE_ROW_MAINLINE_RHODES3_DEV_FW_44.00.01_572492a66aac2909.h>
#endif

#ifdef H1_IOT_SR150_MAINLINE_PROD_FW_46_41_05_6e5b54433f6445e9_bin
#   include <H1_IOT.SR150_MAINLINE_PROD_FW_46.41.05_6e5b54433f6445e9.h>
#endif

#ifdef H1_IOT_SR150_MAINLINE_DEV_FW_46_41_05_6e5b54433f6445e9_bin
#   include <H1_IOT.SR150_MAINLINE_DEV_FW_46.41.05_6e5b54433f6445e9.h>
#endif

#ifdef SR140_SR150_FW_BX_RHODES3_20220316_a39c6fcb_B1v32_00_FB_bin
#   include <SR140_SR150_FW_BX_RHODES3_20220316_a39c6fcb_B1v32.00.FB.h>
#endif

#ifdef SR140_SR150_FW_BX_RHODES3_PROD_20220316_a39c6fcb_B1v32_00_FB_bin
#   include <SR140_SR150_FW_BX_RHODES3_PROD_20220316_a39c6fcb_B1v32.00.FB.h>
#endif

#ifdef H1_SR150_IOT_MAINLINE_PROD_AD_21_03_3ada1397911328db_bin
#   include <H1.SR150_IOT_MAINLINE_PROD_AD.21.03_3ada1397911328db.h>
#endif

#ifdef H1_SR150_IOT_MAINLINE_DEV_AD_21_03_3ada1397911328db_bin
#   include <H1.SR150_IOT_MAINLINE_DEV_AD.21.03_3ada1397911328db.h>
#endif

#ifdef H1_MOBILE_ROW_MAINLINE_PROD_FW_EE_20_A0_a8b28afc11bdaf6c_bin
#   include <H1_MOBILE_ROW_MAINLINE_PROD_FW_EE.20.A0_a8b28afc11bdaf6c.h>
#endif

#ifdef H1_MOBILE_ROW_MAINLINE_DEV_FW_EE_20_A0_a8b28afc11bdaf6c_bin
#   include <H1_MOBILE_ROW_MAINLINE_DEV_FW_EE.20.A0_a8b28afc11bdaf6c.h>
#endif

#ifdef H1_IOT_SR150_MAINLINE_PROD_FW_EE_40_A0_a8b28afc11bdaf6c_bin
#   include <H1_IOT.SR150_MAINLINE_PROD_FW_EE.40.A0_a8b28afc11bdaf6c.h>
#endif

#ifdef H1_IOT_SR150_MAINLINE_DEV_FW_EE_40_A0_a8b28afc11bdaf6c_bin
#   include <H1_IOT.SR150_MAINLINE_DEV_FW_EE.40.A0_a8b28afc11bdaf6c.h>
#endif

#ifdef H1_IOT_SR150_MAINLINE_PROD_FW_46_41_02_d983ae9b7f25963d_bin
#   include <H1_IOT.SR150_MAINLINE_PROD_FW_46.41.02_d983ae9b7f25963d.h>
#endif

#ifdef H1_IOT_SR150_MAINLINE_DEV_FW_46_41_02_d983ae9b7f25963d_bin
#   include <H1_IOT.SR150_MAINLINE_DEV_FW_46.41.02_d983ae9b7f25963d.h>
#endif

#ifdef H1_IOT_SR150_MAINLINE_PROD_FW_EE_40_02_746b01d41a254ace_bin
#   include <H1_IOT.SR150_MAINLINE_PROD_FW_EE.40.02_746b01d41a254ace.h>
#endif

#ifdef H1_IOT_SR150_MAINLINE_DEV_FW_EE_40_02_746b01d41a254ace_bin
#   include <H1_IOT.SR150_MAINLINE_DEV_FW_EE.40.02_746b01d41a254ace.h>
#endif

#ifdef H1_IOT_SR150_MAINLINE_PROD_FW_46_41_04_fd77c7cbfb1b28ee_bin
#   include <H1_IOT.SR150_MAINLINE_PROD_FW_46.41.04_fd77c7cbfb1b28ee.h>
#endif

#ifdef H1_IOT_SR150_MAINLINE_DEV_FW_46_41_04_fd77c7cbfb1b28ee_bin
#   include <H1_IOT.SR150_MAINLINE_DEV_FW_46.41.04_fd77c7cbfb1b28ee.h>
#endif

#endif // END
