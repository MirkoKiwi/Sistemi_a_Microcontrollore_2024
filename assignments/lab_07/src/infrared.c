void init_timer(int counterValue) {
    // Reset the timer and load the initial value
    XTmrCtr_SetControlStatusReg(XPAR_AXI_TIMER_0_BASEADDR, TmrCtrNumber, TIMER_CTRL_RESET);
    XTmrCtr_SetLoadReg(XPAR_AXI_TIMER_0_BASEADDR, TmrCtrNumber, counterValue);
    XTmrCtr_LoadTimerCounterReg(XPAR_AXI_TIMER_0_BASEADDR, TmrCtrNumber);

    // Enable the timer
    XTmrCtr_Enable(XPAR_AXI_TIMER_0_BASEADDR, TmrCtrNumber);
}

void timerEnable(void) {
    // Ensure the timer is running
    u32 controlStatus = XTmrCtr_GetControlStatusReg(XPAR_AXI_TIMER_0_BASEADDR, TmrCtrNumber);
    if (!(controlStatus & XTC_CSR_ENABLE_TMR_MASK)) {
        XTmrCtr_Enable(XPAR_AXI_TIMER_0_BASEADDR, TmrCtrNumber);
    }
}

void timerReset() {
    // Disable and reset the timer
    XTmrCtr_Disable(XPAR_AXI_TIMER_0_BASEADDR, TmrCtrNumber);
    XTmrCtr_SetLoadReg(XPAR_AXI_TIMER_0_BASEADDR, TmrCtrNumber, 0xFFFFFFFF); // Set max value
    XTmrCtr_LoadTimerCounterReg(XPAR_AXI_TIMER_0_BASEADDR, TmrCtrNumber);
}

void captureRawIR() {
    int start = 0;
    u32 high, low;

    while (!start) {
        u32 value;

        while (!(*AXI_GPIO_IR));

        timerEnable();
        low = XTmrCtr_GetTimerCounterReg(XPAR_AXI_TIMER_0_BASEADDR, TmrCtrNumber);

        while (*AXI_GPIO_IR);
        high = XTmrCtr_GetTimerCounterReg(XPAR_AXI_TIMER_0_BASEADDR, TmrCtrNumber);

        value = high - low;
        xil_printf("High: %d | Low: %d | Val: %d\n", high, low, value);

        timerReset();

        if ((value > 400000) && (value < 600000)) {
            start = 1;
        }
    }

    xil_printf("Start of capture\n");
    for (int i = 0; i < 32; i++) {
        while (!(*AXI_GPIO_IR));

        timerEnable();
        low = XTmrCtr_GetTimerCounterReg(XPAR_AXI_TIMER_0_BASEADDR, TmrCtrNumber);

        while (*AXI_GPIO_IR);

        high = XTmrCtr_GetTimerCounterReg(XPAR_AXI_TIMER_0_BASEADDR, TmrCtrNumber);

        data[i] = high - low;

        timerReset();
    }
}

// Funzione per stampare la sequenza
void PrintSequence(u32 data, u32 durations[]) {
    xil_printf("Bit | Numero | Durata (us)\n");
    for (int i = 31; i >= 0; i--) {
        int bit = (data >> i) & 1;
        xil_printf("%2d  | Bit %2d | %10d\n", bit, 32 - i, durations[31 - i] / (TIMER_CLOCK_FREQ_HZ / 1000000));
    }
}

// Decodifica e stampa dati NEC
void decodeAndPrintNECData(u32 data) {
    unsigned char address = (data >> 24) & 0xFF;
    unsigned char address_inv = (data >> 16) & 0xFF;
    unsigned char command = (data >> 8) & 0xFF;
    unsigned char command_inv = data & 0xFF;

    xil_printf("Dati Decodificati:\n");
    xil_printf("Indirizzo: 0x%02X\n", address);
    xil_printf("Indirizzo Inverso: 0x%02X\n", !address_inv);
    xil_printf("Comando: 0x%02X\n", command);
    xil_printf("Comando Inverso: 0x%02X\n", !command_inv);
}
