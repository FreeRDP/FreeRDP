export class IdGenerator {
  private static nextValue: number = 1;

  static next(prefix: string): string {
    const value: number = IdGenerator.nextValue;
    IdGenerator.nextValue = IdGenerator.nextValue + 1;
    return `${prefix}-${value}`;
  }
}
