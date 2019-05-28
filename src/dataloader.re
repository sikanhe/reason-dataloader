type task('key, 'value) = {
  key: 'key,
  resolve: 'value => unit,
};

type t('key, 'value) = {
  load: 'key => Future.t('value),
  loadMany: list('key) => Future.t(list('value)),
  run: unit => unit,
  clearCache: unit => unit,
  prime: ('key, 'value) => unit,
  clear: 'key => unit,
};

[@bs.val] external nextTick: (unit => unit) => unit = "process.nextTick";

module Future = {
  include Future;

  let rec all =
    fun
    | [] => value([])
    | [future, ...rest] =>
      flatMap(future, value => map(all(rest), acc => [value, ...acc]));
};

let make = (~cacheSize=256, batchFn) => {
  let queue = Belt.MutableQueue.make();
  let cache = Hashtbl.create(cacheSize);

  let resolvedPromise = Js.Promise.resolve();

  let runAfterPromiseQueue = fn => {
    resolvedPromise
    |> Js.Promise.then_(() => Js.Promise.resolve(nextTick(fn)))
    |> ignore;
  };

  let run = () => {
    switch (Belt.MutableQueue.peek(queue)) {
    | None => ()
    | Some(_) =>
      let tasks = queue |> Belt.MutableQueue.toArray;
      Belt.MutableQueue.clear(queue);

      let keys = tasks |> Array.map(task => task.key);

      keys
      |> Array.to_list
      |> batchFn
      |> Future.get(_, values =>
           switch (
             List.combine(tasks->Array.to_list, values)
             |> List.iter(((task, result)) => task.resolve(result))
           ) {
           | r => r
           | exception (Invalid_argument(_)) =>
             Js.Exn.raiseTypeError(
               "DataLoader must be constructed with a function which accepts "
               ++ "list('key) and returns Future.t(list('value)), but the function did "
               ++ "not return a Future of a list of the same length as the list "
               ++ "of keys."
               ++ "\n\nKeys:\n"
               ++ keys->Js.Array.toString
               ++ "\n\nValues:\n"
               ++ values->Array.of_list->Js.Array.toString,
             )
           }
         );
    };
  };

  let load = key => {
    switch (Hashtbl.find(cache, key)) {
    | future => future
    | exception Not_found =>
      let future =
        Future.make(resolve => {
          Belt.MutableQueue.add(queue, {key, resolve});
          // queue := [{key, resolve}, ...queue^];
          switch (Belt.MutableQueue.size(queue)) {
          | 1 => runAfterPromiseQueue(run)
          | _ => ()
          };
        });
      Hashtbl.add(cache, key, future);
      future;
    };
  };

  let loadMany = keys => Future.all(List.map(load, keys));

  let prime = (key, value) => Hashtbl.add(cache, key, Future.value(value));
  let clear = key => Hashtbl.remove(cache, key);
  let clearCache = () => Hashtbl.clear(cache);

  {load, loadMany, run, clear, clearCache, prime};
};