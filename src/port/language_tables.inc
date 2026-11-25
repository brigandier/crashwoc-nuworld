/* Menu and HUD text slots. The original six language columns stay intact. */
#ifdef NW_LANGUAGE_DECLARE
extern char *HubName[6][6];
extern char *LevelName[44][6];
extern char *PlaceName[5][6];
extern char *PlaceName3[5][6];
extern char *tNEWGAME[6];
extern char *tDEMO[6];
extern char *tBONUS[6];
extern char *tCHECK[6];
extern char *tPOINT[6];
extern char *tENTERINGTIMETRIAL[6];
extern char *tRESUME[6];
extern char *tOPTIONS[6];
extern char *tQUIT[6];
extern char *tWARPROOM[6];
extern char *tRESTARTTRIAL[6];
extern char *tRESTARTRACE[6];
extern char *tBESTTIME[6];
extern char *t2NDBESTTIME[6];
extern char *t3RDBESTTIME[6];
extern char *tNONEWTIME[6];
extern char *tCONTINUE[6];
extern char *tGAMEOVER[6];
extern char *tYES[6];
extern char *tNO[6];
extern char *tON[6];
extern char *tOFF[6];
extern char *tGO[6];
extern char *tDONE[6];
extern char *tCANCEL[6];
extern char *tEXIT[6];
extern char *tVIBRATION[6];
extern char *tSOUND[6];
extern char *tSOUNDOPTIONS[6];
extern char *tSURROUND[6];
extern char *tSFXVOLUME[6];
extern char *tMUSICVOLUME[6];
extern char *tSTERO[6];
extern char *tMONO[6];
extern char *tLANGUAGE[6];
extern char *tRELICSREQUIRED[6];
extern char *tCRYSTALS[6];
extern char *tGEMS[6];
extern char *tRELICS[6];
extern char *tTIMES[6];
extern char *tPOWERS[6];
extern char *tSUPERBELLYFLOP[6];
extern char *tSUPERBELLYFLOPTEXT[6][7];
extern char *tDEATHTORNADOSPIN[6];
extern char *tDEATHTORNADOSPINTEXT[6][7];
extern char *tDOUBLEJUMP[6];
extern char *tDOUBLEJUMPTEXT[6][7];
extern char *tFRUITBAZOOKA[6];
extern char *tFRUITBAZOOKATEXT[6][7];
extern char *tSPEEDSHOES[6];
extern char *tSPEEDSHOESTEXT[6][7];
extern char *tSNEAKSHOES[6];
extern char *tSNEAKSHOESTEXT[6][7];
extern char *tPRESSxTOCONTINUE[6];
extern char *tSELECTALEVELANDPRESSxTOPLAY[6];
extern char *tPRESSSTARTBUTTON[6];
extern char *tPRESSANYBUTTON[6];
extern char *tADJUSTSCREEN[6];
extern char *tHORIZONTAL[6];
extern char *tVERTICAL[6];
extern char *tINSERTCONTROLLERINPORT1[6][2];
extern char *tEMPTY[6];
extern char *tLOAD[6];
extern char *tSAVE[6];
extern char *tDELETE[6];
extern char *tLOADGAME[6];
extern char *tSAVEGAME[6];
extern char *tDELETEGAME[6];
extern char *tNODATAAVAILABLE[6];
extern char *tOVERWRITE[6];
extern char *tCONFIRMLOAD[6];
extern char *tCONFIRMSAVE[6];
extern char *tCONFIRMDELETE[6];
extern char *tCONFIRMOVERWRITE[6][2];
extern char *tLOADING[6];
extern char *tSAVING[6];
extern char *tDELETING[6];
extern char *tGAMELOADED[6];
extern char *tGAMESAVED[6];
extern char *tGAMEDELETED[6];
extern char *tFAILEDTOLOAD[6];
extern char *tFAILEDTOSAVE[6];
extern char *tFAILEDTODELETE[6];
#else
    { &PlaceName[0][0], NULL },
    { &PlaceName[1][0], NULL },
    { &PlaceName[2][0], NULL },
    { &PlaceName[3][0], NULL },
    { &PlaceName[4][0], NULL },
    { &PlaceName3[0][0], NULL },
    { &PlaceName3[1][0], NULL },
    { &PlaceName3[2][0], NULL },
    { &PlaceName3[3][0], NULL },
    { &PlaceName3[4][0], NULL },
    { &tNEWGAME[0], NULL },
    { &tDEMO[0], NULL },
    { &tBONUS[0], NULL },
    { &tCHECK[0], NULL },
    { &tPOINT[0], NULL },
    { &tENTERINGTIMETRIAL[0], NULL },
    { &tRESUME[0], NULL },
    { &tOPTIONS[0], NULL },
    { &tQUIT[0], NULL },
    { &tRESTARTTRIAL[0], NULL },
    { &tRESTARTRACE[0], NULL },
    { &tBESTTIME[0], NULL },
    { &t2NDBESTTIME[0], NULL },
    { &t3RDBESTTIME[0], NULL },
    { &tNONEWTIME[0], NULL },
    { &tCONTINUE[0], NULL },
    { &tGAMEOVER[0], NULL },
    { &tYES[0], NULL },
    { &tNO[0], NULL },
    { &tON[0], NULL },
    { &tOFF[0], NULL },
    { &tGO[0], NULL },
    { &tDONE[0], NULL },
    { &tCANCEL[0], NULL },
    { &tEXIT[0], NULL },
    { &tVIBRATION[0], NULL },
    { &tSOUND[0], NULL },
    { &tSOUNDOPTIONS[0], NULL },
    { &tSURROUND[0], NULL },
    { &tSFXVOLUME[0], NULL },
    { &tMUSICVOLUME[0], NULL },
    { &tSTERO[0], NULL },
    { &tMONO[0], NULL },
    { &tLANGUAGE[0], NULL },
    { &tRELICSREQUIRED[0], NULL },
    { &tCRYSTALS[0], NULL },
    { &tGEMS[0], NULL },
    { &tRELICS[0], NULL },
    { &tTIMES[0], NULL },
    { &tPOWERS[0], NULL },
    { &tSUPERBELLYFLOP[0], NULL },
    { &tSUPERBELLYFLOPTEXT[0][0], NULL },
    { &tSUPERBELLYFLOPTEXT[0][1], NULL },
    { &tSUPERBELLYFLOPTEXT[0][2], NULL },
    { &tSUPERBELLYFLOPTEXT[0][3], NULL },
    { &tSUPERBELLYFLOPTEXT[0][4], NULL },
    { &tSUPERBELLYFLOPTEXT[0][5], NULL },
    { &tSUPERBELLYFLOPTEXT[0][6], NULL },
    { &tDEATHTORNADOSPIN[0], NULL },
    { &tDEATHTORNADOSPINTEXT[0][0], NULL },
    { &tDEATHTORNADOSPINTEXT[0][1], NULL },
    { &tDEATHTORNADOSPINTEXT[0][2], NULL },
    { &tDEATHTORNADOSPINTEXT[0][3], NULL },
    { &tDEATHTORNADOSPINTEXT[0][4], NULL },
    { &tDEATHTORNADOSPINTEXT[0][5], NULL },
    { &tDEATHTORNADOSPINTEXT[0][6], NULL },
    { &tDOUBLEJUMP[0], NULL },
    { &tDOUBLEJUMPTEXT[0][0], NULL },
    { &tDOUBLEJUMPTEXT[0][1], NULL },
    { &tDOUBLEJUMPTEXT[0][2], NULL },
    { &tDOUBLEJUMPTEXT[0][3], NULL },
    { &tDOUBLEJUMPTEXT[0][4], NULL },
    { &tDOUBLEJUMPTEXT[0][5], NULL },
    { &tDOUBLEJUMPTEXT[0][6], NULL },
    { &tFRUITBAZOOKA[0], NULL },
    { &tFRUITBAZOOKATEXT[0][0], NULL },
    { &tFRUITBAZOOKATEXT[0][1], NULL },
    { &tFRUITBAZOOKATEXT[0][2], NULL },
    { &tFRUITBAZOOKATEXT[0][3], NULL },
    { &tFRUITBAZOOKATEXT[0][4], NULL },
    { &tFRUITBAZOOKATEXT[0][5], NULL },
    { &tFRUITBAZOOKATEXT[0][6], NULL },
    { &tSPEEDSHOES[0], NULL },
    { &tSPEEDSHOESTEXT[0][0], NULL },
    { &tSPEEDSHOESTEXT[0][1], NULL },
    { &tSPEEDSHOESTEXT[0][2], NULL },
    { &tSPEEDSHOESTEXT[0][3], NULL },
    { &tSPEEDSHOESTEXT[0][4], NULL },
    { &tSPEEDSHOESTEXT[0][5], NULL },
    { &tSPEEDSHOESTEXT[0][6], NULL },
    { &tSNEAKSHOES[0], NULL },
    { &tSNEAKSHOESTEXT[0][0], NULL },
    { &tSNEAKSHOESTEXT[0][1], NULL },
    { &tSNEAKSHOESTEXT[0][2], NULL },
    { &tSNEAKSHOESTEXT[0][3], NULL },
    { &tSNEAKSHOESTEXT[0][4], NULL },
    { &tSNEAKSHOESTEXT[0][5], NULL },
    { &tSNEAKSHOESTEXT[0][6], NULL },
    { &tPRESSxTOCONTINUE[0], NULL },
    { &tSELECTALEVELANDPRESSxTOPLAY[0], NULL },
    { &tPRESSSTARTBUTTON[0], NULL },
    { &tPRESSANYBUTTON[0], NULL },
    { &tADJUSTSCREEN[0], NULL },
    { &tHORIZONTAL[0], NULL },
    { &tVERTICAL[0], NULL },
    { &tINSERTCONTROLLERINPORT1[0][0], NULL },
    { &tINSERTCONTROLLERINPORT1[0][1], NULL },
    { &tEMPTY[0], NULL },
    { &tLOAD[0], NULL },
    { &tSAVE[0], NULL },
    { &tDELETE[0], NULL },
    { &tLOADGAME[0], NULL },
    { &tSAVEGAME[0], NULL },
    { &tDELETEGAME[0], NULL },
    { &tNODATAAVAILABLE[0], NULL },
    { &tOVERWRITE[0], NULL },
    { &tCONFIRMLOAD[0], NULL },
    { &tCONFIRMSAVE[0], NULL },
    { &tCONFIRMDELETE[0], NULL },
    { &tCONFIRMOVERWRITE[0][0], NULL },
    { &tCONFIRMOVERWRITE[0][1], NULL },
    { &tLOADING[0], NULL },
    { &tSAVING[0], NULL },
    { &tDELETING[0], NULL },
    { &tGAMELOADED[0], NULL },
    { &tGAMESAVED[0], NULL },
    { &tGAMEDELETED[0], NULL },
    { &tFAILEDTOLOAD[0], NULL },
    { &tFAILEDTOSAVE[0], NULL },
    { &tFAILEDTODELETE[0], NULL },
#endif
