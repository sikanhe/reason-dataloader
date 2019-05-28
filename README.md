# Dataloader

This is an implementation of [Dataloader](https://github.com/graphql/dataloader) in Reason using [Future](https://github.com/RationalJS/future) as the async primitive.

- Original Implemetation: https://github.com/graphql/dataloader
- Future library used: https://github.com/RationalJS/future

Checkout `src/dataloader.rei` for the full API interface

# Usage

```reason
module Future = FutureLoader.Future;

let userLoader = Dataloader.make(userIds => getUsersByIds(userIds));

userLoader.load(1)
->Future.get(user => Js.log2("found user with ID = 1", user));
```

# Caching

Calling the same loader instance with the same keys will result in returning cached futures;

```reason
userLoader.load(1) // Future with key = 1
userLoader.load(1) // will not issue a dispatch and returns the same Future as previous call
```

You can pre-populate the cache with `prime(key, value)`, or clear the cache with `clear(key)` and `clearAll()`.

It is recommended to create new instances of loaders per user request, so 
1) we don't have a global cache that does not get garbage collected
2) Having multiple requests writing/reading from the same cache, resulting in unpredictable behaviors. 