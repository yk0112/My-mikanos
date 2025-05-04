#include "keyboard.hpp"
#include "usb/classdriver/keyboard.hpp"

namespace {
    
    const char keycode_map[256] = {
        0,    0,    0,    0,    'a',  'b',  'c',  'd', 
        'e',  'f',  'g',  'h',  'i',  'j',  'k',  'l',
        'm',  'n',  'o',  'p',  'q',  'r',  's',  't',
        'u',  'v',  'w',  'x',  'y',  'z',  '1',  '2',
        '3',  '4',  '5',  '6',  '7',  '8',  '9',  '0',
        '\n', '\b', 0x08, '\t', ' ',  '-',  '=',  '[',
        ']', '\\',  '#',  ';', '\'',  '`',  ',',  '.',
        '/',  0,    0,    0,    0,    0,    0,    0,
        0,    0,    0,    0,    0,    0,    0,    0,
        0,    0,    0,    0,    0,    0,    0,    0,
        0,    0,    0,    0,    '/',  '*',  '-',  '+',
        '\n', '1',  '2',  '3',  '4',  '5',  '6',  '7',
        '8',  '9',  '0',  '.', '\\',  0,    0,    '=',
      };
}


void InitializeKeyboard(std::deque<Message>& msg_queue) {
    // msg_queueを経由せず、直接文字表示したほうが良いのでは？
    usb::HIDKeyboardDriver::default_observer = 
        [&msg_queue](uint8_t keycode) {
            Message msg{Message::kKeyPush};
            msg.arg.keyboard.keycode = keycode;
            msg.arg.keyboard.ascii = keycode_map[keycode];
            msg_queue.push_back(msg); 
        }; 
}