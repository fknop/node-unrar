export function promisify (bind: any, method: any, args: any): Promise<any> {

  return new Promise((resolve, reject) => {

    const callback = (err: any, result: any) => {

      if (err) {
          return reject(err);
      }

      return resolve(result);
    };

    method.apply(bind, args ? args.concat(callback) : [callback]);
  });
};