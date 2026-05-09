import { util } from '@kit.ArkTS';
import { common } from '@kit.AbilityKit';
import { i18n as systemI18n } from '@kit.LocalizationKit';
import dataPreferences from '@ohos.data.preferences';

const LANGUAGE_PREFERENCE_KEY = '__app_language__';
const PREFS_STORE_NAME = 'freerdp_store';
const PREFS_KEY_LANGUAGE = 'user_language';

export interface LanguageMeta {
  readonly code: string;
  readonly labelKey: string;
  readonly fileName: string;
}

const SUPPORTED_LANGUAGES: LanguageMeta[] = [
  { code: 'zh-CN', labelKey: 'lang_chinese', fileName: 'i18n/zh-CN.json' },
  { code: 'en-US', labelKey: 'lang_english', fileName: 'i18n/en-US.json' },
];

class I18nManager {
  private bundle: Map<string, Record<string, string>> = new Map();
  private fallback: Record<string, string> = {};
  private labels: Map<string, string> = new Map();
  private changeListeners: Array<(code: string) => void> = [];

  get supportedLanguages(): LanguageMeta[] {
    return SUPPORTED_LANGUAGES;
  }

  onLanguageChanged(listener: (code: string) => void): void {
    this.changeListeners.push(listener);
  }

  init(context: common.Context): void {
    const zhFallback = this.loadJsonFile(context, 'i18n/zh-CN.json');
    if (zhFallback && zhFallback.strings) {
      this.fallback = zhFallback.strings as Record<string, string>;
    } else {
      this.fallback = this.buildFallbackZhStrings();
    }

    for (const meta of SUPPORTED_LANGUAGES) {
      const data = this.loadJsonFile(context, meta.fileName);
      if (data && data.strings) {
        this.bundle.set(meta.code, data.strings as Record<string, string>);
      }
      if (data && data.language && (data.language as Record<string, string>).label) {
        this.labels.set(meta.code, (data.language as Record<string, string>).label);
      }
    }

    if (!this.bundle.has('zh-CN')) {
      this.bundle.set('zh-CN', this.fallback);
    }

    this.loadFromPreferences(context);
  }

  setLanguage(code: string): void {
    AppStorage.setOrCreate(LANGUAGE_PREFERENCE_KEY, code);
    AppStorage.setOrCreate('appName', this.get(code, 'app_title'));
    this.saveToPreferences(code);
    for (const listener of this.changeListeners) {
      listener(code);
    }
  }

  private loadFromPreferences(context: common.Context): void {
    try {
      const pref = dataPreferences.getPreferencesSync(context, { name: PREFS_STORE_NAME });
      const saved = pref.getSync(PREFS_KEY_LANGUAGE, 'system') as string;
      AppStorage.setOrCreate(LANGUAGE_PREFERENCE_KEY, saved);
    } catch (_e) {
      AppStorage.setOrCreate(LANGUAGE_PREFERENCE_KEY, 'system');
    }
  }

  private saveToPreferences(code: string): void {
    try {
      const ctx = AppStorage.get<common.Context>('__app_context__');
      if (ctx) {
        const pref = dataPreferences.getPreferencesSync(ctx, { name: PREFS_STORE_NAME });
        pref.putSync(PREFS_KEY_LANGUAGE, code);
        pref.flush();
      }
    } catch (_e) {
    }
  }

  get(locale: string, key: string): string {
    const resolved: string = this.resolveLocale(locale);
    const strings: Record<string, string> | undefined = this.bundle.get(resolved);
    if (strings !== undefined && strings[key] !== undefined) {
      return strings[key];
    }
    if (this.fallback[key] !== undefined) {
      return this.fallback[key];
    }
    return key;
  }

  getLabel(langCode: string): string {
    return this.labels.get(langCode) || langCode;
  }

  currentLocale(): string {
    const saved = AppStorage.get<string>(LANGUAGE_PREFERENCE_KEY);
    if (saved !== undefined && saved.length > 0) {
      return saved;
    }
    return 'system';
  }

  resolvedLocale(): string {
    return this.resolveLocale(this.currentLocale());
  }

  getFormattedRelativeTime(locale: string, ts: number | undefined): string {
    if (!ts || ts <= 0) {
      return this.get(locale, 'never_used');
    }
    const now = Date.now();
    const diff = now - ts;
    if (diff < 60000) {
      return this.get(locale, 'just_now');
    }
    if (diff < 3600000) {
      return `${Math.floor(diff / 60000)}${this.get(locale, 'minutes_ago')}`;
    }
    if (diff < 86400000) {
      return `${Math.floor(diff / 3600000)}${this.get(locale, 'hours_ago')}`;
    }
    if (diff < 604800000) {
      return `${Math.floor(diff / 86400000)}${this.get(locale, 'days_ago')}`;
    }
    const d = new Date(ts);
    return `${d.getMonth() + 1}/${d.getDate()}`;
  }

  getStageLabel(locale: string, stage: number): string {
    if (stage === 3) {
      return this.get(locale, 'stage_connected');
    }
    if (stage === 2) {
      return this.get(locale, 'stage_connecting');
    }
    if (stage === 4) {
      return this.get(locale, 'stage_disconnected');
    }
    if (stage === 5) {
      return this.get(locale, 'stage_error');
    }
    return this.get(locale, 'stage_idle');
  }

  getSessionCountLabel(locale: string, count: number): string {
    const resolved: string = this.resolveLocale(locale);
    if (resolved === 'en-US') {
      return count === 1 ? '1 session' : `${count} sessions`;
    }
    return `${count} 个`;
  }

  private resolveLocale(locale: string): string {
    if (locale === 'system') {
      return this.detectSystemLocale();
    }
    return locale;
  }

  private detectSystemLocale(): string {
    try {
      const sysLang: string = systemI18n.System.getSystemLanguage();
      if (sysLang.length > 0 && sysLang.startsWith('zh')) {
        return 'zh-CN';
      }
      return 'en-US';
    } catch (_e) {
      return 'zh-CN';
    }
  }

  private loadJsonFile(context: common.Context, path: string): Record<string, Object> | null {
    try {
      const raw: Uint8Array = context.resourceManager.getRawFileContentSync(path);
      const text: string = util.TextDecoder.create('utf-8', { ignoreBOM: true }).decodeToString(raw);
      const parsed: Record<string, Object> = JSON.parse(text) as Record<string, Object>;
      return parsed;
    } catch (_e) {
      return null;
    }
  }

  private buildFallbackZhStrings(): Record<string, string> {
    const s: Record<string, string> = {};
    s['app_brand'] = '碧虎';
    s['app_desktop'] = '远程桌面';
    s['app_title'] = '碧虎远程桌面';
    s['app_subtitle'] = '便利的远程访问';
    s['connection_settings'] = '连接设置';
    s['new_connection'] = '新建';
    s['connection_name_placeholder'] = '连接名称';
    s['server_address_placeholder'] = '服务器地址';
    s['port_placeholder'] = '端口';
    s['username_placeholder'] = '用户名';
    s['domain_optional_placeholder'] = '域（可选）';
    s['password_placeholder'] = '密码';
    s['empty_credentials_hint'] = '不填写凭据时，将显示远端系统登录界面，可直接在远端输入。';
    s['remember_password'] = '记住密码';
    s['gateway_settings'] = '网关设置';
    s['gateway_configured'] = '已配置';
    s['gateway_address_placeholder'] = '网关地址（host 或 host:port）';
    s['gateway_username_placeholder'] = '网关用户名';
    s['gateway_domain_placeholder'] = '网关域';
    s['gateway_password_placeholder'] = '网关密码';
    s['security_mode'] = '安全模式';
    s['security_auto'] = '自动';
    s['clipboard_bidirectional'] = '双向剪贴板';
    s['save_config'] = '保存配置';
    s['connect_desktop'] = '连接桌面';
    s['cert_verify'] = '证书验证';
    s['cert_unable_verify'] = '无法验证此远程计算机的身份，是否仍要连接？';
    s['cert_reject'] = '拒绝连接';
    s['cert_accept'] = '接受并连接';
    s['cert_hostname'] = '主机名';
    s['cert_subject'] = '主题';
    s['cert_issuer'] = '颁发者';
    s['cert_fingerprint'] = '指纹';
    s['auth_required'] = '需要身份验证';
    s['auth_credentials_prompt'] = '远程桌面服务器需要您的凭据';
    s['cancel'] = '取消';
    s['confirm'] = '确定';
    s['fullscreen'] = '全屏';
    s['exit_fullscreen'] = '退出全屏';
    s['keyboard'] = '键盘';
    s['disconnect'] = '断开';
    s['stage_connected'] = '已连接';
    s['stage_connecting'] = '连接中';
    s['stage_disconnected'] = '已断开';
    s['stage_error'] = '错误';
    s['stage_idle'] = '空闲';
    s['not_connected'] = '尚未连接';
    s['connecting_msg'] = '正在连接远程桌面...';
    s['disconnected_msg'] = '连接已断开';
    s['sent_ctrl_alt_del'] = '已发送 Ctrl+Alt+Del';
    s['sent_windows_key'] = '已发送 Windows 键';
    s['sent_escape'] = '已发送 Escape';
    s['no_session_id'] = '未找到会话标识';
    s['create_session_failed'] = '创建会话失败';
    s['open_window_failed'] = '打开远程桌面窗口失败';
    s['about'] = '关于';
    s['active_sessions'] = '活跃会话';
    s['search_placeholder'] = '搜索：名称 / 地址 / 账号';
    s['clear'] = '清除';
    s['no_bookmarks'] = '暂无连接配置';
    s['no_search_results'] = '未找到匹配的连接';
    s['bookmarks_empty_hint'] = '点击下方按钮创建你的第一个远程桌面连接';
    s['search_no_match_hint'] = '尝试其他关键词搜索';
    s['new_connection_btn'] = '新增连接';
    s['edit_connection'] = '编辑连接';
    s['delete'] = '删除';
    s['confirm_delete'] = '确认删除';
    s['confirm_delete_msg'] = '确定要删除此连接配置吗？此操作无法撤销。';
    s['switch_session'] = '切换';
    s['switch_session_short'] = '切';
    s['disconnect_session'] = '断开';
    s['disconnect_session_short'] = '断';
    s['load_config'] = '载入配置';
    s['bookmarks_empty_title'] = '暂无保存的连接';
    s['bookmarks_empty_subtitle'] = '在「新建」标签页填写配置并保存';
    s['gateway_label_prefix'] = '网关：';
    s['never_used'] = '从未使用';
    s['just_now'] = '刚刚';
    s['minutes_ago'] = ' 分钟前';
    s['hours_ago'] = ' 小时前';
    s['days_ago'] = ' 天前';
    s['company_info'] = '公司信息';
    s['dev_team'] = '开发团队';
    s['dev_team_value'] = '横渠天地（北京）科技有限公司';
    s['business_cooperation'] = '商务合作';
    s['official_website'] = '官方网站';
    s['agreements_privacy'] = '协议与隐私';
    s['privacy_policy'] = '隐私协议';
    s['user_agreement'] = '用户协议';
    s['view_full_content'] = '查看完整内容';
    s['open_source_license'] = '开源许可';
    s['license_statement'] = '许可声明';
    s['license_statement_value'] = '本应用采用 Apache-2.0 许可发布，基于 FreeRDP 开源项目。';
    s['source_repo'] = '源码仓库';
    s['third_party_components'] = '第三方组件';
    s['third_party_value'] = 'FreeRDP (Apache-2.0) · OpenSSL (Apache-2.0)';
    s['disclaimer'] = '免责声明';
    s['disclaimer_value'] = '本软件按"现状"提供，不附带任何明示或暗示担保。';
    s['visit_link'] = '访问链接';
    s['copy_text'] = '复制文本';
    s['agreements_title'] = '协议阅读与确认';
    s['agree_continue'] = '同意并继续';
    s['waiting_connect'] = '等待连接';
    s['bookmark_service_not_initialized'] = '书签服务尚未初始化';
    s['bookmark_saved'] = '书签已保存';
    s['loaded_profile_prefix'] = '已载入';
    s['please_fill_host_and_user'] = '请填写服务器地址和用户名';
    s['connected_to'] = '已连接到';
    s['remote_window_opened'] = '，远程桌面窗口已打开';
    s['connect_failed_prefix'] = '连接失败: ';
    s['language'] = '语言';
    s['lang_chinese'] = '中文';
    s['lang_english'] = 'English';
    s['connecting_first_frame'] = '正在建立会话并等待首帧...';
    s['waiting_first_frame'] = '已连接，正在等待首帧...';
    s['connect_to_show_desktop'] = '连接后将在这里展示远程桌面画面';
    s['agreement_loading'] = '协议内容加载中...';
    s['exit_app'] = '退出应用';
    s['close'] = '关闭';
    s['agreement_load_failed'] = '协议内容加载失败，请稍后重试。';
    s['agreement_launch_prompt'] = '首次使用前，请完整阅读并确认《碧虎远程桌面隐私协议》与《碧虎远程桌面用户协议》，同意后方可继续使用应用。';
    s['agreement_review_hint'] = '以下内容与首启展示保持一致，支持在应用内随时查看完整文本。';
    s['copied_to_clipboard'] = '已复制';
    s['select_language'] = '选择语言';
    s['follow_system'] = '跟随系统';
    return s;
  }
}

export const i18n = new I18nManager();
