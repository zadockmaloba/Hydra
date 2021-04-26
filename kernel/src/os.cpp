#include "os.h"

PageTableManager pageTableManager = NULL;
IDTR idtr;

int InitOS(BootData* bootdata) {
    if (bootdata->font == NULL) return 1;
    if (bootdata->framebuffer == NULL) return 2;
    if (bootdata->Map == NULL) return 3;
    GDTDescriptor gdtDescriptor;
    gdtDescriptor.Size = sizeof(GDT)-1;
    gdtDescriptor.Offset = (uint64_t) &DefaultGDT;
    LoadGDT(&gdtDescriptor);
    Allocator = PageFrameAllocator();
    uint64_t MapEntries = bootdata->MapSize / bootdata->MapDescriptorSize;
    Allocator.ReadEFIMemoryMap(bootdata->Map, bootdata->MapSize, bootdata->MapDescriptorSize);
    uint64_t kernelSize = (uint64_t) &KernelEnd - (uint64_t) &KernelStart;
    uint64_t kernelPages = (uint64_t)kernelSize / 4096 + 1;
    Allocator.LockPages(&KernelStart, kernelPages);
    PageTable* PML4 = (PageTable*) Allocator.RequestPage();
    memset(PML4, 0, 0x1000);
    pageTableManager = PageTableManager(PML4);
    uint64_t fbBase = (uint64_t) bootdata->framebuffer->BaseAddress;
    uint64_t fbSize = (uint64_t) bootdata->framebuffer->Size + 0x1000;
    for (uint64_t t = 0; t < getMemorySize(bootdata->Map, MapEntries, bootdata->MapDescriptorSize); t += 0x1000) pageTableManager.MapMemory((void*)t, (void*)t);
    Allocator.LockPages((void*)fbBase, fbSize / 0x1000 + 1);
    for (uint64_t t = fbBase; t < fbBase + fbSize; t += 4096) pageTableManager.MapMemory((void*) t, (void*) t);
    asm ("mov %0, %%cr3" : : "r" (PML4));
    
    idtr.Limit = 0x0FFF;
    idtr.Offset = (uint64_t) Allocator.RequestPage();

    IDTDescEntry* int_PageFault = (IDTDescEntry*)(idtr.Offset + 0xE * sizeof(IDTDescEntry));
    int_PageFault->SetOffset((uint64_t) PageFault_Handler);
    int_PageFault->type_attr = IDT_TA_InterruptGate;
    int_PageFault->selector = 0x08;

    IDTDescEntry* int_DoubleFault = (IDTDescEntry*)(idtr.Offset + 0x8 * sizeof(IDTDescEntry));
    int_DoubleFault->SetOffset((uint64_t) DoubleFault_Handler);
    int_DoubleFault->type_attr = IDT_TA_InterruptGate;
    int_DoubleFault->selector = 0x08;

    IDTDescEntry* int_GPFault = (IDTDescEntry*)(idtr.Offset + 0xD * sizeof(IDTDescEntry));
    int_GPFault->SetOffset((uint64_t) GPFault_Handler);
    int_GPFault->type_attr = IDT_TA_InterruptGate;
    int_GPFault->selector = 0x08;

    IDTDescEntry* int_Keyboard = (IDTDescEntry*)(idtr.Offset + 0x21 * sizeof(IDTDescEntry));
    int_Keyboard->SetOffset((uint64_t) KeyboardInt_Handler);
    int_Keyboard->type_attr = IDT_TA_InterruptGate;
    int_Keyboard->selector = 0x08;

    asm ("lidt %0" : : "m" (idtr));

    RemapPIC();

    outb(PIC1_DATA, 0b11111101);
    outb(PIC2_DATA, 0b11111111);

    asm ("sti");
    return 4664;
}

int StartOS(BootData* bootdata) {
    gui->SetFramebuffer(bootdata->framebuffer);
    gui->SetFont(bootdata->font);
    unsigned int centerX = gui->GetWidth()/2;
    unsigned int centerY = gui->GetHeight()/2;
    gui->DrawRectangleFromTo(0, 0, gui->GetWidth(), gui->GetHeight(), 0x333333);

    unsigned int w = _strlen("BaseAddress: 0x")*8+_strlen(to_hstring((uint64_t)gui->GetBaseAddress()))*8+20;
    unsigned int c = gui->getpixel(gui->GetWidth(), 0)+0xD;
    gui->DrawBox(gui->GetWidth()-w, 0, w, 84, c, 0, 0, 5, 0);
    gui->SetColor(0xFFFFFF);
    gui->SetXY(gui->GetWidth()-w+10, 10);
    gui->printf("Resolution: ");
    gui->SetXY(gui->GetWidth()-10-_strlen(to_string((uint64_t) gui->GetWidth()))*8-8-_strlen(to_string((uint64_t) gui->GetWidth()))*8, 10);
    gui->printf(to_string((uint64_t) gui->GetWidth()));
    gui->printf("x");
    gui->printf(to_string((uint64_t) gui->GetHeight()));
    gui->SetXY(gui->GetWidth()-w+10, 26);
    gui->printf("BaseAddress: 0x");
    gui->printf(to_hstring((uint64_t) gui->GetBaseAddress()));
    gui->SetXY(gui->GetWidth()-w+10, 42);
    gui->printf("PPSL: ");
    gui->SetXY(gui->GetWidth()-10-_strlen(to_string((uint64_t) gui->GetPPSL()))*8, 42);
    gui->printf(to_string((uint64_t) gui->GetPPSL()));
    gui->SetXY(gui->GetWidth()-w+10, 58);
    gui->printf("Size: ");
    gui->SetXY(gui->GetWidth()-10-_strlen(to_string((uint64_t) gui->GetSize()))*8, 58);
    gui->printf(to_string((uint64_t) gui->GetSize()));

    gui->DrawCursor(centerX, centerY, 0);

    gui->SetXY(0,0);

    gui->printf("test\ntest\ntesting");

    // asm("int $0x0e");            Force Panic
    
    while (true) asm("hlt");
    return 300;
}