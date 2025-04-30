struct Message {
    enum Type {
        kInterruptXHCI,
        kInterruptAPICTimer
    } type;
};
