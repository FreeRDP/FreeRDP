export class KeyboardMapper {
  // Defines key states for modifier keys
  public static readonly KEYSTATE_ON = 1;
  public static readonly KEYSTATE_LOCKED = 2;
  public static readonly KEYSTATE_OFF = 3;

  // Basic RDP Scancodes mapping based on FreeRDP
  private static readonly RDP_SCANCODE_ESCAPE = 0x0001;
  private static readonly RDP_SCANCODE_KEY_1 = 0x0002;
  private static readonly RDP_SCANCODE_KEY_2 = 0x0003;
  private static readonly RDP_SCANCODE_KEY_3 = 0x0004;
  private static readonly RDP_SCANCODE_KEY_4 = 0x0005;
  private static readonly RDP_SCANCODE_KEY_5 = 0x0006;
  private static readonly RDP_SCANCODE_KEY_6 = 0x0007;
  private static readonly RDP_SCANCODE_KEY_7 = 0x0008;
  private static readonly RDP_SCANCODE_KEY_8 = 0x0009;
  private static readonly RDP_SCANCODE_KEY_9 = 0x000A;
  private static readonly RDP_SCANCODE_KEY_0 = 0x000B;
  private static readonly RDP_SCANCODE_OEM_MINUS = 0x000C;
  private static readonly RDP_SCANCODE_OEM_PLUS = 0x000D;
  private static readonly RDP_SCANCODE_BACKSPACE = 0x000E;
  private static readonly RDP_SCANCODE_TAB = 0x000F;
  private static readonly RDP_SCANCODE_KEY_Q = 0x0010;
  private static readonly RDP_SCANCODE_KEY_W = 0x0011;
  private static readonly RDP_SCANCODE_KEY_E = 0x0012;
  private static readonly RDP_SCANCODE_KEY_R = 0x0013;
  private static readonly RDP_SCANCODE_KEY_T = 0x0014;
  private static readonly RDP_SCANCODE_KEY_Y = 0x0015;
  private static readonly RDP_SCANCODE_KEY_U = 0x0016;
  private static readonly RDP_SCANCODE_KEY_I = 0x0017;
  private static readonly RDP_SCANCODE_KEY_O = 0x0018;
  private static readonly RDP_SCANCODE_KEY_P = 0x0019;
  private static readonly RDP_SCANCODE_RETURN = 0x001C;
  private static readonly RDP_SCANCODE_LCONTROL = 0x001D;
  private static readonly RDP_SCANCODE_KEY_A = 0x001E;
  private static readonly RDP_SCANCODE_KEY_S = 0x001F;
  private static readonly RDP_SCANCODE_KEY_D = 0x0020;
  private static readonly RDP_SCANCODE_KEY_F = 0x0021;
  private static readonly RDP_SCANCODE_KEY_G = 0x0022;
  private static readonly RDP_SCANCODE_KEY_H = 0x0023;
  private static readonly RDP_SCANCODE_KEY_J = 0x0024;
  private static readonly RDP_SCANCODE_KEY_K = 0x0025;
  private static readonly RDP_SCANCODE_KEY_L = 0x0026;
  private static readonly RDP_SCANCODE_LSHIFT = 0x002A;
  private static readonly RDP_SCANCODE_KEY_Z = 0x002C;
  private static readonly RDP_SCANCODE_KEY_X = 0x002D;
  private static readonly RDP_SCANCODE_KEY_C = 0x002E;
  private static readonly RDP_SCANCODE_KEY_V = 0x002F;
  private static readonly RDP_SCANCODE_KEY_B = 0x0030;
  private static readonly RDP_SCANCODE_KEY_N = 0x0031;
  private static readonly RDP_SCANCODE_KEY_M = 0x0032;
  private static readonly RDP_SCANCODE_SPACE = 0x0039;

  // Extended scancodes
  private static readonly RDP_SCANCODE_UP = 0x0148;
  private static readonly RDP_SCANCODE_LEFT = 0x014B;
  private static readonly RDP_SCANCODE_RIGHT = 0x014D;
  private static readonly RDP_SCANCODE_DOWN = 0x0150;
  private static readonly RDP_SCANCODE_DELETE = 0x0153;
  private static readonly RDP_SCANCODE_LWIN = 0x015B;

  public static mapKeyScancode(keyText: string, keyCode: number): number {
    switch (keyText) {
      case 'Enter': return KeyboardMapper.RDP_SCANCODE_RETURN;
      case 'Backspace': return KeyboardMapper.RDP_SCANCODE_BACKSPACE;
      case 'Tab': return KeyboardMapper.RDP_SCANCODE_TAB;
      case 'Escape': return KeyboardMapper.RDP_SCANCODE_ESCAPE;
      case 'Space': return KeyboardMapper.RDP_SCANCODE_SPACE;
      case 'ArrowLeft': return KeyboardMapper.RDP_SCANCODE_LEFT;
      case 'ArrowRight': return KeyboardMapper.RDP_SCANCODE_RIGHT;
      case 'ArrowUp': return KeyboardMapper.RDP_SCANCODE_UP;
      case 'ArrowDown': return KeyboardMapper.RDP_SCANCODE_DOWN;
      case 'Delete': return KeyboardMapper.RDP_SCANCODE_DELETE;
      case 'Shift': return KeyboardMapper.RDP_SCANCODE_LSHIFT;
      case 'Control': return KeyboardMapper.RDP_SCANCODE_LCONTROL;
      case 'Meta': return KeyboardMapper.RDP_SCANCODE_LWIN;
      default: break;
    }

    if (keyText.length === 1) {
      const upper = keyText.toUpperCase();
      switch (upper) {
        case 'A': return KeyboardMapper.RDP_SCANCODE_KEY_A;
        case 'B': return KeyboardMapper.RDP_SCANCODE_KEY_B;
        case 'C': return KeyboardMapper.RDP_SCANCODE_KEY_C;
        case 'D': return KeyboardMapper.RDP_SCANCODE_KEY_D;
        case 'E': return KeyboardMapper.RDP_SCANCODE_KEY_E;
        case 'F': return KeyboardMapper.RDP_SCANCODE_KEY_F;
        case 'G': return KeyboardMapper.RDP_SCANCODE_KEY_G;
        case 'H': return KeyboardMapper.RDP_SCANCODE_KEY_H;
        case 'I': return KeyboardMapper.RDP_SCANCODE_KEY_I;
        case 'J': return KeyboardMapper.RDP_SCANCODE_KEY_J;
        case 'K': return KeyboardMapper.RDP_SCANCODE_KEY_K;
        case 'L': return KeyboardMapper.RDP_SCANCODE_KEY_L;
        case 'M': return KeyboardMapper.RDP_SCANCODE_KEY_M;
        case 'N': return KeyboardMapper.RDP_SCANCODE_KEY_N;
        case 'O': return KeyboardMapper.RDP_SCANCODE_KEY_O;
        case 'P': return KeyboardMapper.RDP_SCANCODE_KEY_P;
        case 'Q': return KeyboardMapper.RDP_SCANCODE_KEY_Q;
        case 'R': return KeyboardMapper.RDP_SCANCODE_KEY_R;
        case 'S': return KeyboardMapper.RDP_SCANCODE_KEY_S;
        case 'T': return KeyboardMapper.RDP_SCANCODE_KEY_T;
        case 'U': return KeyboardMapper.RDP_SCANCODE_KEY_U;
        case 'V': return KeyboardMapper.RDP_SCANCODE_KEY_V;
        case 'W': return KeyboardMapper.RDP_SCANCODE_KEY_W;
        case 'X': return KeyboardMapper.RDP_SCANCODE_KEY_X;
        case 'Y': return KeyboardMapper.RDP_SCANCODE_KEY_Y;
        case 'Z': return KeyboardMapper.RDP_SCANCODE_KEY_Z;
        case '1': return KeyboardMapper.RDP_SCANCODE_KEY_1;
        case '2': return KeyboardMapper.RDP_SCANCODE_KEY_2;
        case '3': return KeyboardMapper.RDP_SCANCODE_KEY_3;
        case '4': return KeyboardMapper.RDP_SCANCODE_KEY_4;
        case '5': return KeyboardMapper.RDP_SCANCODE_KEY_5;
        case '6': return KeyboardMapper.RDP_SCANCODE_KEY_6;
        case '7': return KeyboardMapper.RDP_SCANCODE_KEY_7;
        case '8': return KeyboardMapper.RDP_SCANCODE_KEY_8;
        case '9': return KeyboardMapper.RDP_SCANCODE_KEY_9;
        case '0': return KeyboardMapper.RDP_SCANCODE_KEY_0;
        case '-': return KeyboardMapper.RDP_SCANCODE_OEM_MINUS;
        case '=': return KeyboardMapper.RDP_SCANCODE_OEM_PLUS;
      }
    }
    return 0;
  }
}
