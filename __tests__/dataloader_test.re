open Jest;

module Future = {
  include Future;

  let rec all =
    fun
    | [] => value([])
    | [future, ...rest] =>
      flatMap(future, value => map(all(rest), acc => [value, ...acc]));
};

module IdentityLoader = {
  let make = () => {
    let batchCalls = ref([]);
    let loader =
      Dataloader.make(keys => {
        batchCalls := List.append(batchCalls^, [keys]);
        Future.value(keys);
      });

    (loader, batchCalls);
  };
};

describe("Primary API", () => {
  open Expect;
  open! Expect.Operators;

  testAsync("works for one key", assertion => {
    let identityLoader = Dataloader.make(keys => Future.value(keys));

    identityLoader.load(1)->Future.get(one => assertion(expect(one) == 1));
  });

  testAsync("works for many keys in one call", assertion => {
    let identityLoader = Dataloader.make(keys => Future.value(keys));

    identityLoader.load([1, 2, 3])
    ->Future.get(nums => assertion(expect(nums) == [1, 2, 3]));
  });

  test("coalesces identical requests", () => {
    let identityLoader = Dataloader.make(keys => Future.value(keys));

    let future1 = identityLoader.load(1);
    let future2 = identityLoader.load(1);

    expect(future1) === future2;
  });

  testAsync("batches multiple requests", assertion => {
    let (identityLoader, batchCalls) = IdentityLoader.make();

    let future1 = identityLoader.load(1);
    let future2 = identityLoader.load(2);

    Future.all([future1, future2])
    ->Future.get(_results => assertion(expect(batchCalls^) == [[1, 2]]));
  });

  testAsync("caches repeated requests", assertion => {
    let (identityLoader, batchCalls) = IdentityLoader.make();

    let futureA = identityLoader.load("A");
    let futureB = identityLoader.load("B");

    Future.all([futureA, futureB])
    ->Future.get(_ => {
        let futureA2 = identityLoader.load("A");
        let futureC = identityLoader.load("C");

        Future.all([futureA2, futureC])
        ->Future.get(_ => {
            let futureA3 = identityLoader.load("A");
            let futureB2 = identityLoader.load("B");
            let futureC2 = identityLoader.load("C");

            Future.all([futureA3, futureC2, futureB2])
            ->Future.get(_ =>
                assertion(expect(batchCalls^) == [["A", "B"], ["C"]])
              );
          });
      });
  });
});

describe("It is resilient to job queue ordering", () => {
  open Expect;
  open! Expect.Operators;

  testAsync("batch loads occur during promises", assertion => {
    let (identityLoader, batchCalls) = IdentityLoader.make();

    Js.Promise.(
      all([|
        identityLoader.load("A")->FutureJs.toPromise,
        resolve()
        |> then_(() => resolve())
        |> then_(() => {
             identityLoader.load("B")->ignore;
             resolve()
             |> then_(() => resolve())
             |> then_(() => {
                  identityLoader.load("C")->ignore;
                  resolve()
                  |> then_(() => resolve())
                  |> then_(() => identityLoader.load("D")->FutureJs.toPromise);
                });
           }),
      |])
      |> then_(_ => {
           assertion(expect(batchCalls^) == [["A", "B", "C", "D"]]);
           resolve();
         })
    )
    |> ignore;
  });

  testAsync("batch loads occur during futures", assertion => {
    let (identityLoader, batchCalls) = IdentityLoader.make();

    Future.(
      all([
        identityLoader.load("A"),
        value()
        ->flatMap(() => value())
        ->flatMap(() => {
            identityLoader.load("B")->ignore;
            value()
            ->flatMap(() => value())
            ->flatMap(_ => {
                identityLoader.load("C")->ignore;
                value()
                ->flatMap(() => value())
                ->flatMap(_ => identityLoader.load("D"));
              });
          }),
      ])
      ->flatMap(_ => {
          assertion(expect(batchCalls^) == [["A", "B", "C", "D"]]);
          value();
        })
    )
    |> ignore;
  });
});