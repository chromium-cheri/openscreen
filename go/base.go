package osp

import (
	"context"
)

func waitUntilDone(ctx context.Context) {
	<-ctx.Done()
}

func done(ctx context.Context) bool {
	return ctx.Err() != nil
}
