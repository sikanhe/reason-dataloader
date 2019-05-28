type task('key, 'value);

type t('key, 'value) = {
  load: 'key => Future.t('value),
  loadMany: list('key) => Future.t(list('value)),
  run: unit => unit,
  clearCache: unit => unit,
  prime: ('key, 'value) => unit,
  clear: 'key => unit,
};

let make:
  (~cacheSize: int=?, list('key) => Future.t(list('value))) =>
  t('key, 'value);
